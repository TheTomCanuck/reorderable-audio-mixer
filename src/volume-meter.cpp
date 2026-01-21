#include "volume-meter.hpp"

#include <util/platform.h>

#include <QPainter>
#include <QPaintEvent>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <limits>

// Size of the input indicator in pixels
#define INDICATOR_THICKNESS 3

// Padding on top and bottom of vertical meters
#define METER_PADDING 1

VolumeMeter::VolumeMeter(QWidget *parent, bool vert)
	: QWidget(parent),
	  vertical(vert)
{
	setAttribute(Qt::WA_OpaquePaintEvent, true);

	tickFont = font();
	tickFont.setPointSizeF(tickFont.pointSizeF() * 0.7);
	QFontMetrics metrics(tickFont);

	// Set minimum size based on orientation and channel count
	if (vertical) {
		// Match OBS calculation: meter width + tick marks + scale label width + spacing
		int meterWidth = displayNrAudioChannels * (meterThickness + 1) - 1;
		QRect scaleBounds = metrics.boundingRect("-88");
		setMinimumSize(meterWidth + 10 + scaleBounds.width() + 2, 100);
		setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
	} else {
		int minHeight = displayNrAudioChannels * (meterThickness + 1) - 1 + 4 + metrics.capHeight();
		setMinimumSize(100, minHeight);
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	}

	resetLevels();

	// Update timer for smooth animation (~60fps)
	updateTimer = new QTimer(this);
	connect(updateTimer, &QTimer::timeout, this, [this]() {
		update();
	});
	updateTimer->start(16);
}

void VolumeMeter::setVertical(bool vert)
{
	if (vertical == vert)
		return;

	vertical = vert;

	// Update size policy - match OBS calculation
	QFontMetrics metrics(tickFont);
	if (vertical) {
		int meterWidth = displayNrAudioChannels * (meterThickness + 1) - 1;
		QRect scaleBounds = metrics.boundingRect("-88");
		setMinimumSize(meterWidth + 10 + scaleBounds.width() + 2, 100);
		setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
	} else {
		int minHeight = displayNrAudioChannels * (meterThickness + 1) - 1 + 4 + metrics.capHeight();
		setMinimumSize(100, minHeight);
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	}

	updateGeometry();
	update();
}

VolumeMeter::~VolumeMeter()
{
	updateTimer->stop();
}

void VolumeMeter::setLevels(const float magnitude[MAX_AUDIO_CHANNELS],
			    const float peak[MAX_AUDIO_CHANNELS],
			    const float inputPeak[MAX_AUDIO_CHANNELS])
{
	uint64_t ts = os_gettime_ns();
	QMutexLocker locker(&dataMutex);

	currentLastUpdateTime = ts;
	for (int i = 0; i < MAX_AUDIO_CHANNELS; i++) {
		currentMagnitude[i] = magnitude[i];
		currentPeak[i] = peak[i];
		currentInputPeak[i] = inputPeak[i];
	}
}

void VolumeMeter::resetLevels()
{
	currentLastUpdateTime = 0;
	for (int i = 0; i < MAX_AUDIO_CHANNELS; i++) {
		currentMagnitude[i] = -INFINITY;
		currentPeak[i] = -INFINITY;
		currentInputPeak[i] = -INFINITY;

		displayMagnitude[i] = -INFINITY;
		displayPeak[i] = -INFINITY;
		displayPeakHold[i] = -INFINITY;
		displayPeakHoldLastUpdateTime[i] = 0;
		displayInputPeakHold[i] = -INFINITY;
		displayInputPeakHoldLastUpdateTime[i] = 0;
	}
}

void VolumeMeter::calculateBallisticsForChannel(int channelNr, qreal timeSinceLastRedraw)
{
	uint64_t ts = os_gettime_ns();

	if (currentPeak[channelNr] >= displayPeak[channelNr] ||
	    std::isnan(displayPeak[channelNr])) {
		// Attack of peak is immediate
		displayPeak[channelNr] = currentPeak[channelNr];
	} else {
		// Decay
		float decay = float(peakDecayRate * timeSinceLastRedraw);
		displayPeak[channelNr] = std::clamp(
			displayPeak[channelNr] - decay,
			std::min(currentPeak[channelNr], 0.f), 0.f);
	}

	if (currentPeak[channelNr] >= displayPeakHold[channelNr] ||
	    !std::isfinite(displayPeakHold[channelNr])) {
		// Attack of peak-hold is immediate
		displayPeakHold[channelNr] = currentPeak[channelNr];
		displayPeakHoldLastUpdateTime[channelNr] = ts;
	} else {
		// Peak hold falls back after duration
		qreal timeSinceLastPeak =
			(ts - displayPeakHoldLastUpdateTime[channelNr]) * 0.000000001;
		if (timeSinceLastPeak > peakHoldDuration) {
			displayPeakHold[channelNr] = currentPeak[channelNr];
			displayPeakHoldLastUpdateTime[channelNr] = ts;
		}
	}

	if (currentInputPeak[channelNr] >= displayInputPeakHold[channelNr] ||
	    !std::isfinite(displayInputPeakHold[channelNr])) {
		displayInputPeakHold[channelNr] = currentInputPeak[channelNr];
		displayInputPeakHoldLastUpdateTime[channelNr] = ts;
	} else {
		qreal timeSinceLastPeak =
			(ts - displayInputPeakHoldLastUpdateTime[channelNr]) * 0.000000001;
		if (timeSinceLastPeak > inputPeakHoldDuration) {
			displayInputPeakHold[channelNr] = currentInputPeak[channelNr];
			displayInputPeakHoldLastUpdateTime[channelNr] = ts;
		}
	}

	if (!std::isfinite(displayMagnitude[channelNr])) {
		displayMagnitude[channelNr] = currentMagnitude[channelNr];
	} else {
		// VU meter integration
		float attack = float((currentMagnitude[channelNr] - displayMagnitude[channelNr]) *
				     (timeSinceLastRedraw / magnitudeIntegrationTime) * 0.99);
		displayMagnitude[channelNr] = std::clamp(
			displayMagnitude[channelNr] + attack,
			(float)minimumLevel, 0.f);
	}
}

void VolumeMeter::calculateBallistics(qreal timeSinceLastRedraw)
{
	QMutexLocker locker(&dataMutex);

	for (int i = 0; i < MAX_AUDIO_CHANNELS; i++) {
		calculateBallisticsForChannel(i, timeSinceLastRedraw);
	}
}

int VolumeMeter::convertToInt(float number)
{
	constexpr int min = std::numeric_limits<int>::min();
	constexpr int max = std::numeric_limits<int>::max();

	if (number >= (float)max)
		return max;
	else if (number < min)
		return min;
	else
		return int(number);
}

void VolumeMeter::paintInputMeter(QPainter &painter, int x, int y,
				  int width, int height, float peakHold)
{
	QColor color;

	if (peakHold < minimumInputLevel)
		color = backgroundNominalColor;
	else if (peakHold < warningLevel)
		color = foregroundNominalColor;
	else if (peakHold < errorLevel)
		color = foregroundWarningColor;
	else if (peakHold <= clipLevel)
		color = foregroundErrorColor;
	else
		color = clipColor;

	painter.fillRect(x, y, width, height, color);
}

void VolumeMeter::paintTicks(QPainter &painter, int x, int y, int width)
{
	qreal scale = width / minimumLevel;

	painter.setFont(tickFont);
	QFontMetrics metrics(tickFont);
	painter.setPen(majorTickColor);

	// Draw major tick lines and numeric indicators
	for (int i = 0; i >= minimumLevel; i -= 5) {
		int position = int(x + width - (i * scale) - 1);
		QString str = QString::number(i);

		// Center the number on the tick
		QRect textBounds = metrics.boundingRect(str);
		int pos;
		if (i == 0) {
			pos = position - textBounds.width();
		} else {
			pos = position - (textBounds.width() / 2);
			if (pos < 0)
				pos = 0;
		}
		painter.drawText(pos, y + 4 + metrics.capHeight(), str);
		painter.drawLine(position, y, position, y + 2);
	}
}

void VolumeMeter::paintMeter(QPainter &painter, int x, int y, int width, int height,
			     float magnitude, float peak, float peakHold)
{
	qreal scale = width / minimumLevel;

	QMutexLocker locker(&dataMutex);
	int minimumPosition = x + 0;
	int maximumPosition = x + width;
	int magnitudePosition = x + width - convertToInt(magnitude * scale);
	int peakPosition = x + width - convertToInt(peak * scale);
	int peakHoldPosition = x + width - convertToInt(peakHold * scale);
	int warningPosition = x + width - convertToInt(warningLevel * scale);
	int errorPosition = x + width - convertToInt(errorLevel * scale);

	int nominalLength = warningPosition - minimumPosition;
	int warningLength = errorPosition - warningPosition;
	int errorLength = maximumPosition - errorPosition;
	locker.unlock();

	if (clipping) {
		peakPosition = maximumPosition;
	}

	if (peakPosition < minimumPosition) {
		// Below minimum - show all background
		painter.fillRect(minimumPosition, y, nominalLength, height,
				 muted ? backgroundNominalColorDisabled : backgroundNominalColor);
		painter.fillRect(warningPosition, y, warningLength, height,
				 muted ? backgroundWarningColorDisabled : backgroundWarningColor);
		painter.fillRect(errorPosition, y, errorLength, height,
				 muted ? backgroundErrorColorDisabled : backgroundErrorColor);
	} else if (peakPosition < warningPosition) {
		// In nominal range
		painter.fillRect(minimumPosition, y, peakPosition - minimumPosition, height,
				 muted ? foregroundNominalColorDisabled : foregroundNominalColor);
		painter.fillRect(peakPosition, y, warningPosition - peakPosition, height,
				 muted ? backgroundNominalColorDisabled : backgroundNominalColor);
		painter.fillRect(warningPosition, y, warningLength, height,
				 muted ? backgroundWarningColorDisabled : backgroundWarningColor);
		painter.fillRect(errorPosition, y, errorLength, height,
				 muted ? backgroundErrorColorDisabled : backgroundErrorColor);
	} else if (peakPosition < errorPosition) {
		// In warning range
		painter.fillRect(minimumPosition, y, nominalLength, height,
				 muted ? foregroundNominalColorDisabled : foregroundNominalColor);
		painter.fillRect(warningPosition, y, peakPosition - warningPosition, height,
				 muted ? foregroundWarningColorDisabled : foregroundWarningColor);
		painter.fillRect(peakPosition, y, errorPosition - peakPosition, height,
				 muted ? backgroundWarningColorDisabled : backgroundWarningColor);
		painter.fillRect(errorPosition, y, errorLength, height,
				 muted ? backgroundErrorColorDisabled : backgroundErrorColor);
	} else if (peakPosition < maximumPosition) {
		// In error range
		painter.fillRect(minimumPosition, y, nominalLength, height,
				 muted ? foregroundNominalColorDisabled : foregroundNominalColor);
		painter.fillRect(warningPosition, y, warningLength, height,
				 muted ? foregroundWarningColorDisabled : foregroundWarningColor);
		painter.fillRect(errorPosition, y, peakPosition - errorPosition, height,
				 muted ? foregroundErrorColorDisabled : foregroundErrorColor);
		painter.fillRect(peakPosition, y, maximumPosition - peakPosition, height,
				 muted ? backgroundErrorColorDisabled : backgroundErrorColor);
	} else {
		// Clipping
		if (!clipping) {
			clipping = true;
			QTimer::singleShot(1000, this, [this]() { clipping = false; });
		}
		int end = errorLength + warningLength + nominalLength;
		painter.fillRect(minimumPosition, y, end, height,
				 muted ? foregroundErrorColorDisabled : foregroundErrorColor);
	}

	// Draw peak hold indicator (3px wide line)
	if (peakHoldPosition - 3 >= minimumPosition) {
		QColor holdColor;
		if (peakHoldPosition < warningPosition)
			holdColor = muted ? foregroundNominalColorDisabled : foregroundNominalColor;
		else if (peakHoldPosition < errorPosition)
			holdColor = muted ? foregroundWarningColorDisabled : foregroundWarningColor;
		else
			holdColor = muted ? foregroundErrorColorDisabled : foregroundErrorColor;

		painter.fillRect(peakHoldPosition - 3, y, 3, height, holdColor);
	}

	// Draw magnitude indicator (black bar)
	if (magnitudePosition - 3 >= minimumPosition) {
		painter.fillRect(magnitudePosition - 3, y, 3, height, magnitudeColor);
	}
}

void VolumeMeter::paintInputMeterVertical(QPainter &painter, int x, int y,
					  int width, int height, float peakHold)
{
	QColor color;

	if (peakHold < minimumInputLevel)
		color = backgroundNominalColor;
	else if (peakHold < warningLevel)
		color = foregroundNominalColor;
	else if (peakHold < errorLevel)
		color = foregroundWarningColor;
	else if (peakHold <= clipLevel)
		color = foregroundErrorColor;
	else
		color = clipColor;

	painter.fillRect(x, y, width, height, color);
}

void VolumeMeter::paintTicksVertical(QPainter &painter, int x, int y, int height)
{
	// Match OBS's paintVTicks exactly
	qreal scale = height / minimumLevel;

	painter.setFont(tickFont);
	QFontMetrics metrics(tickFont);
	painter.setPen(majorTickColor);

	// Draw major tick lines and numeric indicators every 5dB
	for (int i = 0; i >= minimumLevel; i -= 5) {
		int position = y + int(i * scale) + METER_PADDING;
		QString str = QString::number(i);

		// Position text based on dB value
		if (i == 0) {
			painter.drawText(x + 6, position + metrics.capHeight(), str);
		} else {
			painter.drawText(x + 4, position + (metrics.capHeight() / 2), str);
		}

		// Draw tick mark
		painter.drawLine(x, position, x + 2, position);
	}
}

void VolumeMeter::paintMeterVertical(QPainter &painter, int x, int y, int width, int height,
				     float magnitude, float peak, float peakHold)
{
	// Match OBS's paintVMeter exactly - uses same math as horizontal
	// but with Y axis inverted by painter transform in paintEvent
	qreal scale = height / minimumLevel;

	QMutexLocker locker(&dataMutex);
	int minimumPosition = y + 0;
	int maximumPosition = y + height;
	int magnitudePosition = y + height - convertToInt(magnitude * scale);
	int peakPosition = y + height - convertToInt(peak * scale);
	int peakHoldPosition = y + height - convertToInt(peakHold * scale);
	int warningPosition = y + height - convertToInt(warningLevel * scale);
	int errorPosition = y + height - convertToInt(errorLevel * scale);

	int nominalLength = warningPosition - minimumPosition;
	int warningLength = errorPosition - warningPosition;
	int errorLength = maximumPosition - errorPosition;
	locker.unlock();

	if (clipping) {
		peakPosition = maximumPosition;
	}

	if (peakPosition < minimumPosition) {
		painter.fillRect(x, minimumPosition, width, nominalLength,
				 muted ? backgroundNominalColorDisabled : backgroundNominalColor);
		painter.fillRect(x, warningPosition, width, warningLength,
				 muted ? backgroundWarningColorDisabled : backgroundWarningColor);
		painter.fillRect(x, errorPosition, width, errorLength,
				 muted ? backgroundErrorColorDisabled : backgroundErrorColor);
	} else if (peakPosition < warningPosition) {
		painter.fillRect(x, minimumPosition, width, peakPosition - minimumPosition,
				 muted ? foregroundNominalColorDisabled : foregroundNominalColor);
		painter.fillRect(x, peakPosition, width, warningPosition - peakPosition,
				 muted ? backgroundNominalColorDisabled : backgroundNominalColor);
		painter.fillRect(x, warningPosition, width, warningLength,
				 muted ? backgroundWarningColorDisabled : backgroundWarningColor);
		painter.fillRect(x, errorPosition, width, errorLength,
				 muted ? backgroundErrorColorDisabled : backgroundErrorColor);
	} else if (peakPosition < errorPosition) {
		painter.fillRect(x, minimumPosition, width, nominalLength,
				 muted ? foregroundNominalColorDisabled : foregroundNominalColor);
		painter.fillRect(x, warningPosition, width, peakPosition - warningPosition,
				 muted ? foregroundWarningColorDisabled : foregroundWarningColor);
		painter.fillRect(x, peakPosition, width, errorPosition - peakPosition,
				 muted ? backgroundWarningColorDisabled : backgroundWarningColor);
		painter.fillRect(x, errorPosition, width, errorLength,
				 muted ? backgroundErrorColorDisabled : backgroundErrorColor);
	} else if (peakPosition < maximumPosition) {
		painter.fillRect(x, minimumPosition, width, nominalLength,
				 muted ? foregroundNominalColorDisabled : foregroundNominalColor);
		painter.fillRect(x, warningPosition, width, warningLength,
				 muted ? foregroundWarningColorDisabled : foregroundWarningColor);
		painter.fillRect(x, errorPosition, width, peakPosition - errorPosition,
				 muted ? foregroundErrorColorDisabled : foregroundErrorColor);
		painter.fillRect(x, peakPosition, width, maximumPosition - peakPosition,
				 muted ? backgroundErrorColorDisabled : backgroundErrorColor);
	} else {
		if (!clipping) {
			clipping = true;
			QTimer::singleShot(1000, this, [this]() { clipping = false; });
		}
		int end = errorLength + warningLength + nominalLength;
		painter.fillRect(x, minimumPosition, width, end,
				 muted ? foregroundErrorColorDisabled : foregroundErrorColor);
	}

	// Peak hold indicator
	if (peakHoldPosition - 3 < minimumPosition)
		; // Peak-hold below minimum, no drawing
	else if (peakHoldPosition < warningPosition)
		painter.fillRect(x, peakHoldPosition - 3, width, 3,
				 muted ? foregroundNominalColorDisabled : foregroundNominalColor);
	else if (peakHoldPosition < errorPosition)
		painter.fillRect(x, peakHoldPosition - 3, width, 3,
				 muted ? foregroundWarningColorDisabled : foregroundWarningColor);
	else
		painter.fillRect(x, peakHoldPosition - 3, width, 3,
				 muted ? foregroundErrorColorDisabled : foregroundErrorColor);

	// Magnitude indicator
	if (magnitudePosition - 3 >= minimumPosition)
		painter.fillRect(x, magnitudePosition - 3, width, 3, magnitudeColor);
}

void VolumeMeter::paintEvent(QPaintEvent *event)
{
	uint64_t ts = os_gettime_ns();
	qreal timeSinceLastRedraw = (ts - lastRedrawTime) * 0.000000001;

	// Check for idle (no updates for 0.5 seconds)
	bool idle = false;
	{
		QMutexLocker locker(&dataMutex);
		double timeSinceLastUpdate = (ts - currentLastUpdateTime) * 0.000000001;
		if (timeSinceLastUpdate > 0.5) {
			resetLevels();
			idle = true;
		}
	}

	if (!idle) {
		calculateBallistics(timeSinceLastRedraw);
	}

	QRect widgetRect = rect();
	int width = widgetRect.width();
	int height = widgetRect.height();

	QPainter painter(this);

	// Paint background
	QColor background = palette().color(QPalette::ColorRole::Window);
	painter.fillRect(event->region().boundingRect(), background);

	QFontMetrics metrics(tickFont);

	if (vertical) {
		// Vertical mode - match OBS stock meter layout exactly
		// Adjust height for padding
		height -= METER_PADDING * 2;
		int meterHeight = height - (INDICATOR_THICKNESS + 3);

		// Draw tick marks BEFORE coordinate transform (normal Y axis)
		paintTicksVertical(painter,
				   displayNrAudioChannels * (meterThickness + 1) - 1,
				   0,
				   meterHeight);

		// Invert the Y axis to ease the meter math (0 at bottom, increases upward)
		painter.translate(0, height + METER_PADDING);
		painter.scale(1, -1);

		// Draw meters for each channel (side by side)
		for (int channelNr = 0; channelNr < displayNrAudioChannels; channelNr++) {
			// Main meter
			paintMeterVertical(painter,
					   channelNr * (meterThickness + 1),
					   INDICATOR_THICKNESS + 2,
					   meterThickness,
					   meterHeight,
					   displayMagnitude[channelNr],
					   displayPeak[channelNr],
					   displayPeakHold[channelNr]);

			// Input indicator at bottom (which appears at top after Y inversion)
			if (!idle) {
				paintInputMeterVertical(painter,
							channelNr * (meterThickness + 1),
							0,
							meterThickness,
							INDICATOR_THICKNESS,
							displayInputPeakHold[channelNr]);
			}
		}
	} else {
		// Horizontal mode - meters go left to right, channels stacked
		int tickHeight = 4 + metrics.capHeight();
		int meterHeight = height - tickHeight;

		// Draw tick marks and labels
		paintTicks(painter, INDICATOR_THICKNESS + 3,
			   displayNrAudioChannels * (meterThickness + 1) - 1,
			   width - (INDICATOR_THICKNESS + 3));

		// Draw meters for each channel
		for (int channelNr = 0; channelNr < displayNrAudioChannels; channelNr++) {
			paintMeter(painter,
				   INDICATOR_THICKNESS + 2,
				   channelNr * (meterThickness + 1),
				   width - (INDICATOR_THICKNESS + 2),
				   meterThickness,
				   displayMagnitude[channelNr],
				   displayPeak[channelNr],
				   displayPeakHold[channelNr]);

			if (!idle) {
				paintInputMeter(painter,
						0,
						channelNr * (meterThickness + 1),
						INDICATOR_THICKNESS,
						meterThickness,
						displayInputPeakHold[channelNr]);
			}
		}
	}

	lastRedrawTime = ts;
}

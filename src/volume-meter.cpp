#include "volume-meter.hpp"

#include <QPainter>
#include <QDateTime>
#include <cmath>

VolumeMeter::VolumeMeter(QWidget *parent)
	: QWidget(parent)
{
	setMinimumSize(50, 10);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	// Update timer for smooth animation
	updateTimer = new QTimer(this);
	connect(updateTimer, &QTimer::timeout, this, [this]() {
		qint64 now = QDateTime::currentMSecsSinceEpoch();
		float deltaTime = (now - lastUpdateTime) / 1000.0f;
		lastUpdateTime = now;

		if (deltaTime > 0.1f)
			deltaTime = 0.1f;

		calculateBallistics(deltaTime);
		update();
	});

	lastUpdateTime = QDateTime::currentMSecsSinceEpoch();
	updateTimer->start(16); // ~60fps
}

VolumeMeter::~VolumeMeter()
{
	updateTimer->stop();
}

void VolumeMeter::setMuted(bool m)
{
	muted = m;
	update();
}

void VolumeMeter::setLevels(float magnitude, float peak, float inputPeak)
{
	currentMagnitude = magnitude;
	currentPeak = peak;
	currentInputPeak = inputPeak;
}

void VolumeMeter::calculateBallistics(float deltaTime)
{
	// Magnitude (VU-style): fast attack, slower release
	if (currentMagnitude > displayMagnitude) {
		displayMagnitude = currentMagnitude;
	} else {
		float decay = peakDecayRate * deltaTime;
		displayMagnitude = std::max(displayMagnitude - decay, currentMagnitude);
	}

	// Peak: instant attack, decay over time
	if (currentPeak > displayPeak) {
		displayPeak = currentPeak;
		displayPeakHold = currentPeak;
		peakHoldTime = peakHoldDuration;
	} else {
		float decay = peakDecayRate * deltaTime;
		displayPeak = std::max(displayPeak - decay, minimumLevel);

		// Peak hold
		if (peakHoldTime > 0) {
			peakHoldTime -= deltaTime;
		} else {
			displayPeakHold = std::max(displayPeakHold - decay, minimumLevel);
		}
	}

	// Clamp to minimum
	displayMagnitude = std::max(displayMagnitude, minimumLevel);
	displayPeak = std::max(displayPeak, minimumLevel);
	displayPeakHold = std::max(displayPeakHold, minimumLevel);
}

float VolumeMeter::dbToPosition(float db) const
{
	if (db <= minimumLevel)
		return 0.0f;
	if (db >= 0.0f)
		return 1.0f;

	return (db - minimumLevel) / (0.0f - minimumLevel);
}

void VolumeMeter::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	int w = width();
	int h = height();

	// Background
	QColor bgColor = muted ? QColor(40, 40, 40) : QColor(30, 30, 30);
	painter.fillRect(rect(), bgColor);

	if (muted) {
		// Just show muted state
		painter.setPen(QColor(100, 100, 100));
		painter.drawText(rect(), Qt::AlignCenter, "M");
		return;
	}

	// Calculate positions
	float magPos = dbToPosition(displayMagnitude);
	float peakHoldPos = dbToPosition(displayPeakHold);
	float warningPos = dbToPosition(warningLevel);
	float errorPos = dbToPosition(errorLevel);

	int magWidth = static_cast<int>(magPos * w);

	// Draw magnitude bar with color zones
	if (magWidth > 0) {
		int warningX = static_cast<int>(warningPos * w);
		int errorX = static_cast<int>(errorPos * w);

		// Green zone (nominal)
		int greenEnd = std::min(magWidth, warningX);
		if (greenEnd > 0) {
			painter.fillRect(0, 0, greenEnd, h, QColor(76, 175, 80));
		}

		// Yellow zone (warning)
		if (magWidth > warningX) {
			int yellowStart = warningX;
			int yellowEnd = std::min(magWidth, errorX);
			if (yellowEnd > yellowStart) {
				painter.fillRect(yellowStart, 0, yellowEnd - yellowStart, h, QColor(255, 193, 7));
			}
		}

		// Red zone (error)
		if (magWidth > errorX) {
			int redStart = errorX;
			painter.fillRect(redStart, 0, magWidth - redStart, h, QColor(244, 67, 54));
		}
	}

	// Draw peak hold indicator
	if (peakHoldPos > 0.01f) {
		int peakX = static_cast<int>(peakHoldPos * w);
		QColor peakColor = (displayPeakHold >= errorLevel) ? QColor(244, 67, 54) :
		                   (displayPeakHold >= warningLevel) ? QColor(255, 193, 7) :
		                   QColor(200, 200, 200);
		painter.setPen(QPen(peakColor, 2));
		painter.drawLine(peakX, 0, peakX, h);
	}

	// Border
	painter.setPen(QColor(60, 60, 60));
	painter.drawRect(0, 0, w - 1, h - 1);
}

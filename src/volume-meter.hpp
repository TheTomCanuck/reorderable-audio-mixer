#pragma once

#include <obs.h>

#include <QWidget>
#include <QMutex>
#include <QTimer>
#include <QFont>

class VolumeMeter : public QWidget {
	Q_OBJECT

public:
	explicit VolumeMeter(QWidget *parent = nullptr);
	~VolumeMeter();

	void setLevels(const float magnitude[MAX_AUDIO_CHANNELS],
		       const float peak[MAX_AUDIO_CHANNELS],
		       const float inputPeak[MAX_AUDIO_CHANNELS]);

	bool muted = false;

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	void resetLevels();
	void calculateBallistics(qreal timeSinceLastRedraw);
	void calculateBallisticsForChannel(int channelNr, qreal timeSinceLastRedraw);
	void paintMeter(QPainter &painter, int x, int y, int width, int height,
			float magnitude, float peak, float peakHold);
	void paintTicks(QPainter &painter, int x, int y, int width);
	void paintInputMeter(QPainter &painter, int x, int y, int width, int height,
			     float peakHold);
	int convertToInt(float number);

	QMutex dataMutex;

	uint64_t currentLastUpdateTime = 0;
	float currentMagnitude[MAX_AUDIO_CHANNELS];
	float currentPeak[MAX_AUDIO_CHANNELS];
	float currentInputPeak[MAX_AUDIO_CHANNELS];

	int displayNrAudioChannels = 2;
	float displayMagnitude[MAX_AUDIO_CHANNELS];
	float displayPeak[MAX_AUDIO_CHANNELS];
	float displayPeakHold[MAX_AUDIO_CHANNELS];
	uint64_t displayPeakHoldLastUpdateTime[MAX_AUDIO_CHANNELS];
	float displayInputPeakHold[MAX_AUDIO_CHANNELS];
	uint64_t displayInputPeakHoldLastUpdateTime[MAX_AUDIO_CHANNELS];

	QFont tickFont;

	// Colors
	QColor backgroundNominalColor{0x26, 0x7f, 0x26};  // Dark green
	QColor backgroundWarningColor{0x7f, 0x7f, 0x26};  // Dark yellow
	QColor backgroundErrorColor{0x7f, 0x26, 0x26};    // Dark red
	QColor foregroundNominalColor{0x4c, 0xff, 0x4c};  // Bright green
	QColor foregroundWarningColor{0xff, 0xff, 0x4c};  // Bright yellow
	QColor foregroundErrorColor{0xff, 0x4c, 0x4c};    // Bright red
	QColor clipColor{0xff, 0xff, 0xff};               // White
	QColor magnitudeColor{0x00, 0x00, 0x00};          // Black
	QColor majorTickColor{0xff, 0xff, 0xff};          // White
	QColor minorTickColor{0x32, 0x32, 0x32};          // Dark gray

	// Muted colors (all same gray shades - dark for background, light for foreground)
	QColor backgroundNominalColorDisabled{75, 75, 75};
	QColor backgroundWarningColorDisabled{75, 75, 75};
	QColor backgroundErrorColorDisabled{75, 75, 75};
	QColor foregroundNominalColorDisabled{150, 150, 150};
	QColor foregroundWarningColorDisabled{150, 150, 150};
	QColor foregroundErrorColorDisabled{150, 150, 150};

	// Meter settings
	int meterThickness = 3;
	qreal minimumLevel = -60.0;
	qreal warningLevel = -20.0;
	qreal errorLevel = -9.0;
	qreal clipLevel = -0.5;
	qreal minimumInputLevel = -50.0;
	qreal peakDecayRate = 11.76;          // 20 dB / 1.7 sec
	qreal magnitudeIntegrationTime = 0.3; // 99% in 300 ms
	qreal peakHoldDuration = 20.0;        // 20 seconds
	qreal inputPeakHoldDuration = 1.0;    // 1 second

	uint64_t lastRedrawTime = 0;
	bool clipping = false;

	QTimer *updateTimer = nullptr;
};

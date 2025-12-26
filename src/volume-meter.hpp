#pragma once

#include <QWidget>
#include <QTimer>

class VolumeMeter : public QWidget {
	Q_OBJECT

public:
	explicit VolumeMeter(QWidget *parent = nullptr);
	~VolumeMeter();

	void setMuted(bool muted);

public slots:
	void setLevels(float magnitude, float peak, float inputPeak);

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	void calculateBallistics(float timeSinceLastUpdate);
	float dbToPosition(float db) const;

private:
	// Current levels (in dB)
	float currentMagnitude = -60.0f;
	float currentPeak = -60.0f;
	float currentInputPeak = -60.0f;

	// Display levels (with ballistics)
	float displayMagnitude = -60.0f;
	float displayPeak = -60.0f;
	float displayPeakHold = -60.0f;
	float peakHoldTime = 0.0f;

	// Settings
	float minimumLevel = -60.0f;
	float warningLevel = -20.0f;
	float errorLevel = -9.0f;
	float clipLevel = -0.5f;
	float peakDecayRate = 23.53f;  // dB/second
	float peakHoldDuration = 1.0f; // seconds

	bool muted = false;

	// Timer for animation
	QTimer *updateTimer = nullptr;
	qint64 lastUpdateTime = 0;
};

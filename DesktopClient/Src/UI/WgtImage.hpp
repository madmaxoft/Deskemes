#pragma once





#include <QWidget>





/** A simple widget that displays a single pixmap, scaled to fit while preserving aspect ratio.
The image is drawn with NO antialiasing. */
class WgtImage:
	public QWidget
{
	using Super = QWidget;

	Q_OBJECT
	Q_DISABLE_COPY(WgtImage)


public:

	explicit WgtImage(QWidget * aParent = nullptr);

	/** Returns the pixmap displayed in the widget. */
	const QPixmap & pixmap() const
	{
		return mPixmap;
	}

	/** Sets the image to be displayed in the widget. */
	void setPixmap(const QPixmap & aPixmap)
	{
		mPixmap = aPixmap;
		update();
	}


private:

	/** The pixmap displayed in the widget. */
	QPixmap mPixmap;


	// QWidget overrides:
	void paintEvent(QPaintEvent * aEvent) override;
};

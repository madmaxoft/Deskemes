#include "WgtImage.hpp"
#include <QPainter>
#include <QStyle>





WgtImage::WgtImage(QWidget * aParent):
	Super(aParent)
{

}





void WgtImage::paintEvent(QPaintEvent * aEvent)
{
	Q_UNUSED(aEvent);
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing, false);
	style()->drawItemPixmap(&painter, rect(), Qt::AlignCenter, mPixmap.scaled(rect().size(), Qt::KeepAspectRatio));
}

/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWidgets module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qcolordialog.h"

#ifndef QT_NO_COLORDIALOG

#include "qapplication.h"
#include "qdesktopwidget.h"
#include "qdrawutil.h"
#include "qevent.h"
#include "qimage.h"
#include "qdrag.h"
#include "qlabel.h"
#include "qlayout.h"
#include "qlineedit.h"
#include "qmenu.h"
#include "qpainter.h"
#include "qpixmap.h"
#include "qpushbutton.h"
#include "qsettings.h"
#include "qsharedpointer.h"
#include "qstyle.h"
#include "qstyleoption.h"
#include "qvalidator.h"
#include "qmimedata.h"
#include "qspinbox.h"
#include "qdialogbuttonbox.h"
#include "qscreen.h"
#include "qcursor.h"
#include "qtimer.h"
#include "qwindow.h"

#include "private/qdialog_p.h"

#include <algorithm>

QT_BEGIN_NAMESPACE

namespace {
class QColorLuminancePicker;
class QColorPicker;
class QColorShower;
class QWellArray;
class QColorPickingEventFilter;
} // unnamed namespace

class QColorDialogPrivate : public QDialogPrivate
{
    Q_DECLARE_PUBLIC(QColorDialog)

public:
    enum SetColorMode {
        ShowColor = 0x1,
        SelectColor = 0x2,
        SetColorAll = ShowColor | SelectColor
    };

    QColorDialogPrivate() : options(QColorDialogOptions::create())
#ifdef Q_OS_WIN32
        , updateTimer(0)
#endif
    {}

    QPlatformColorDialogHelper *platformColorDialogHelper() const
        { return static_cast<QPlatformColorDialogHelper *>(platformHelper()); }

    void init(const QColor &initial);
    void initWidgets();
    QRgb currentColor() const;
    QColor currentQColor() const;
    void setCurrentColor(const QColor &color, SetColorMode setColorMode = SetColorAll);
    void setCurrentRgbColor(QRgb rgb);
    void setCurrentQColor(const QColor &color);
    bool selectColor(const QColor &color);
    QColor grabScreenColor(const QPoint &p);

    int currentAlpha() const;
    void setCurrentAlpha(int a);
    void showAlpha(bool b);
    bool isAlphaVisible() const;
    void retranslateStrings();

    void _q_addCustom();

    void _q_newHsv(int h, int s, int v);
    void _q_newColorTypedIn(QRgb rgb);
    void _q_nextCustom(int, int);
    void _q_newCustom(int, int);
    void _q_newStandard(int, int);
    void _q_pickScreenColor();
    void _q_updateColorPicking();
    void updateColorLabelText(const QPoint &);
    void updateColorPicking(const QPoint &pos);
    void releaseColorPicking();
    bool handleColorPickingMouseMove(QMouseEvent *e);
    bool handleColorPickingMouseButtonRelease(QMouseEvent *e);
    bool handleColorPickingKeyPress(QKeyEvent *e);

    bool canBeNativeDialog() const Q_DECL_OVERRIDE;

    QWellArray *custom;
    QWellArray *standard;

    QDialogButtonBox *buttons;
    QVBoxLayout *leftLay;
    QColorPicker *cp;
    QColorLuminancePicker *lp;
    QColorShower *cs;
    QLabel *lblBasicColors;
    QLabel *lblCustomColors;
    QLabel *lblScreenColorInfo;
    QPushButton *ok;
    QPushButton *cancel;
    QPushButton *addCusBt;
    QPushButton *screenColorPickerButton;
    QColor selectedQColor;
    int nextCust;
    bool smallDisplay;
    bool screenColorPicking;
    QColorPickingEventFilter *colorPickingEventFilter;
    QRgb beforeScreenColorPicking;
    QSharedPointer<QColorDialogOptions> options;

    QPointer<QObject> receiverToDisconnectOnClose;
    QByteArray memberToDisconnectOnClose;
#ifdef Q_OS_WIN32
    QTimer *updateTimer;
    QWindow dummyTransparentWindow;
#endif

private:
    virtual void initHelper(QPlatformDialogHelper *h) Q_DECL_OVERRIDE;
    virtual void helperPrepareShow(QPlatformDialogHelper *h) Q_DECL_OVERRIDE;
};

//////////// QWellArray BEGIN

namespace {

struct QWellArrayData;

class QWellArray : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int selectedColumn READ selectedColumn)
    Q_PROPERTY(int selectedRow READ selectedRow)

public:
    QWellArray(int rows, int cols, QWidget* parent=0);
    ~QWellArray() {}
    QString cellContent(int row, int col) const;

    int selectedColumn() const { return selCol; }
    int selectedRow() const { return selRow; }

    virtual void setCurrent(int row, int col);
    virtual void setSelected(int row, int col);

    QSize sizeHint() const Q_DECL_OVERRIDE;

    virtual void setCellBrush(int row, int col, const QBrush &);
    QBrush cellBrush(int row, int col);

    inline int cellWidth() const
        { return cellw; }

    inline int cellHeight() const
        { return cellh; }

    inline int rowAt(int y) const
        { return y / cellh; }

    inline int columnAt(int x) const
        { if (isRightToLeft()) return ncols - (x / cellw) - 1; return x / cellw; }

    inline int rowY(int row) const
        { return cellh * row; }

    inline int columnX(int column) const
        { if (isRightToLeft()) return cellw * (ncols - column - 1); return cellw * column; }

    inline int numRows() const
        { return nrows; }

    inline int numCols() const
        {return ncols; }

    inline QRect cellRect() const
        { return QRect(0, 0, cellw, cellh); }

    inline QSize gridSize() const
        { return QSize(ncols * cellw, nrows * cellh); }

    QRect cellGeometry(int row, int column)
        {
            QRect r;
            if (row >= 0 && row < nrows && column >= 0 && column < ncols)
                r.setRect(columnX(column), rowY(row), cellw, cellh);
            return r;
        }

    inline void updateCell(int row, int column) { update(cellGeometry(row, column)); }

signals:
    void selected(int row, int col);
    void currentChanged(int row, int col);

protected:
    virtual void paintCell(QPainter *, int row, int col, const QRect&);
    virtual void paintCellContents(QPainter *, int row, int col, const QRect&);

    void mousePressEvent(QMouseEvent*) Q_DECL_OVERRIDE;
    void mouseReleaseEvent(QMouseEvent*) Q_DECL_OVERRIDE;
    void keyPressEvent(QKeyEvent*) Q_DECL_OVERRIDE;
    void focusInEvent(QFocusEvent*) Q_DECL_OVERRIDE;
    void focusOutEvent(QFocusEvent*) Q_DECL_OVERRIDE;
    void paintEvent(QPaintEvent *) Q_DECL_OVERRIDE;

private:
    Q_DISABLE_COPY(QWellArray)

    int nrows;
    int ncols;
    int cellw;
    int cellh;
    int curRow;
    int curCol;
    int selRow;
    int selCol;
    QWellArrayData *d;
};

void QWellArray::paintEvent(QPaintEvent *e)
{
    QRect r = e->rect();
    int cx = r.x();
    int cy = r.y();
    int ch = r.height();
    int cw = r.width();
    int colfirst = columnAt(cx);
    int collast = columnAt(cx + cw);
    int rowfirst = rowAt(cy);
    int rowlast = rowAt(cy + ch);

    if (isRightToLeft()) {
        int t = colfirst;
        colfirst = collast;
        collast = t;
    }

    QPainter painter(this);
    QPainter *p = &painter;
    QRect rect(0, 0, cellWidth(), cellHeight());


    if (collast < 0 || collast >= ncols)
        collast = ncols-1;
    if (rowlast < 0 || rowlast >= nrows)
        rowlast = nrows-1;

    // Go through the rows
    for (int r = rowfirst; r <= rowlast; ++r) {
        // get row position and height
        int rowp = rowY(r);

        // Go through the columns in the row r
        // if we know from where to where, go through [colfirst, collast],
        // else go through all of them
        for (int c = colfirst; c <= collast; ++c) {
            // get position and width of column c
            int colp = columnX(c);
            // Translate painter and draw the cell
            rect.translate(colp, rowp);
            paintCell(p, r, c, rect);
            rect.translate(-colp, -rowp);
        }
    }
}

struct QWellArrayData {
    QBrush *brush;
};

QWellArray::QWellArray(int rows, int cols, QWidget *parent)
    : QWidget(parent)
        ,nrows(rows), ncols(cols)
{
    d = 0;
    setFocusPolicy(Qt::StrongFocus);
    cellw = 28;
    cellh = 24;
    curCol = 0;
    curRow = 0;
    selCol = -1;
    selRow = -1;
}

QSize QWellArray::sizeHint() const
{
    ensurePolished();
    return gridSize().boundedTo(QSize(640, 480));
}


void QWellArray::paintCell(QPainter* p, int row, int col, const QRect &rect)
{
    int b = 3; //margin

    const QPalette & g = palette();
    QStyleOptionFrame opt;
    int dfw = style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
    opt.lineWidth = dfw;
    opt.midLineWidth = 1;
    opt.rect = rect.adjusted(b, b, -b, -b);
    opt.palette = g;
    opt.state = QStyle::State_Enabled | QStyle::State_Sunken;
    style()->drawPrimitive(QStyle::PE_Frame, &opt, p, this);
    b += dfw;

    if ((row == curRow) && (col == curCol)) {
        if (hasFocus()) {
            QStyleOptionFocusRect opt;
            opt.palette = g;
            opt.rect = rect;
            opt.state = QStyle::State_None | QStyle::State_KeyboardFocusChange;
            style()->drawPrimitive(QStyle::PE_FrameFocusRect, &opt, p, this);
        }
    }
    paintCellContents(p, row, col, opt.rect.adjusted(dfw, dfw, -dfw, -dfw));
}

/*!
  Reimplement this function to change the contents of the well array.
 */
void QWellArray::paintCellContents(QPainter *p, int row, int col, const QRect &r)
{
    if (d) {
        p->fillRect(r, d->brush[row*numCols()+col]);
    } else {
        p->fillRect(r, Qt::white);
        p->setPen(Qt::black);
        p->drawLine(r.topLeft(), r.bottomRight());
        p->drawLine(r.topRight(), r.bottomLeft());
    }
}

void QWellArray::mousePressEvent(QMouseEvent *e)
{
    // The current cell marker is set to the cell the mouse is pressed in
    QPoint pos = e->pos();
    setCurrent(rowAt(pos.y()), columnAt(pos.x()));
}

void QWellArray::mouseReleaseEvent(QMouseEvent * /* event */)
{
    // The current cell marker is set to the cell the mouse is clicked in
    setSelected(curRow, curCol);
}


/*
  Sets the cell currently having the focus. This is not necessarily
  the same as the currently selected cell.
*/

void QWellArray::setCurrent(int row, int col)
{
    if ((curRow == row) && (curCol == col))
        return;

    if (row < 0 || col < 0)
        row = col = -1;

    int oldRow = curRow;
    int oldCol = curCol;

    curRow = row;
    curCol = col;

    updateCell(oldRow, oldCol);
    updateCell(curRow, curCol);

    emit currentChanged(curRow, curCol);
}

/*
  Sets the currently selected cell to \a row, \a column. If \a row or
  \a column are less than zero, the current cell is unselected.

  Does not set the position of the focus indicator.
*/
void QWellArray::setSelected(int row, int col)
{
    int oldRow = selRow;
    int oldCol = selCol;

    if (row < 0 || col < 0)
        row = col = -1;

    selCol = col;
    selRow = row;

    updateCell(oldRow, oldCol);
    updateCell(selRow, selCol);
    if (row >= 0)
        emit selected(row, col);

#ifndef QT_NO_MENU
    if (isVisible() && qobject_cast<QMenu*>(parentWidget()))
        parentWidget()->close();
#endif
}

void QWellArray::focusInEvent(QFocusEvent*)
{
    updateCell(curRow, curCol);
    emit currentChanged(curRow, curCol);
}

void QWellArray::setCellBrush(int row, int col, const QBrush &b)
{
    if (!d) {
        d = new QWellArrayData;
        int i = numRows()*numCols();
        d->brush = new QBrush[i];
    }
    if (row >= 0 && row < numRows() && col >= 0 && col < numCols())
        d->brush[row*numCols()+col] = b;
}

/*
  Returns the brush set for the cell at \a row, \a column. If no brush is
  set, Qt::NoBrush is returned.
*/

QBrush QWellArray::cellBrush(int row, int col)
{
    if (d && row >= 0 && row < numRows() && col >= 0 && col < numCols())
        return d->brush[row*numCols()+col];
    return Qt::NoBrush;
}



/*!\reimp
*/

void QWellArray::focusOutEvent(QFocusEvent*)
{
    updateCell(curRow, curCol);
}

/*\reimp
*/
void QWellArray::keyPressEvent(QKeyEvent* e)
{
    switch(e->key()) {                        // Look at the key code
    case Qt::Key_Left:                                // If 'left arrow'-key,
        if(curCol > 0)                        // and cr't not in leftmost col
            setCurrent(curRow, curCol - 1);        // set cr't to next left column
        break;
    case Qt::Key_Right:                                // Correspondingly...
        if(curCol < numCols()-1)
            setCurrent(curRow, curCol + 1);
        break;
    case Qt::Key_Up:
        if(curRow > 0)
            setCurrent(curRow - 1, curCol);
        break;
    case Qt::Key_Down:
        if(curRow < numRows()-1)
            setCurrent(curRow + 1, curCol);
        break;
#if 0
    // bad idea that shouldn't have been implemented; very counterintuitive
    case Qt::Key_Return:
    case Qt::Key_Enter:
        /*
          ignore the key, so that the dialog get it, but still select
          the current row/col
        */
        e->ignore();
        // fallthrough intended
#endif
    case Qt::Key_Space:
        setSelected(curRow, curCol);
        break;
    default:                                // If not an interesting key,
        e->ignore();                        // we don't accept the event
        return;
    }

}

//////////// QWellArray END

// Event filter to be installed on the dialog while in color-picking mode.
class QColorPickingEventFilter : public QObject {
public:
    explicit QColorPickingEventFilter(QColorDialogPrivate *dp, QObject *parent = 0) : QObject(parent), m_dp(dp) {}

    bool eventFilter(QObject *, QEvent *event) Q_DECL_OVERRIDE
    {
        switch (event->type()) {
        case QEvent::MouseMove:
            return m_dp->handleColorPickingMouseMove(static_cast<QMouseEvent *>(event));
        case QEvent::MouseButtonRelease:
            return m_dp->handleColorPickingMouseButtonRelease(static_cast<QMouseEvent *>(event));
        case QEvent::KeyPress:
            return m_dp->handleColorPickingKeyPress(static_cast<QKeyEvent *>(event));
        default:
            break;
        }
        return false;
    }

private:
    QColorDialogPrivate *m_dp;
};

} // unnamed namespace

/*!
    Returns the number of custom colors supported by QColorDialog. All
    color dialogs share the same custom colors.
*/
int QColorDialog::customCount()
{
    return QColorDialogOptions::customColorCount();
}

/*!
    \since 4.5

    Returns the custom color at the given \a index as a QColor value.
*/
QColor QColorDialog::customColor(int index)
{
    return QColor(QColorDialogOptions::customColor(index));
}

/*!
    Sets the custom color at \a index to the QColor \a color value.

    \note This function does not apply to the Native Color Dialog on the Mac
    OS X platform. If you still require this function, use the
    QColorDialog::DontUseNativeDialog option.
*/
void QColorDialog::setCustomColor(int index, QColor color)
{
    QColorDialogOptions::setCustomColor(index, color.rgba());
}

/*!
    \since 5.0

    Returns the standard color at the given \a index as a QColor value.
*/
QColor QColorDialog::standardColor(int index)
{
    return QColor(QColorDialogOptions::standardColor(index));
}

/*!
    Sets the standard color at \a index to the QColor \a color value.

    \note This function does not apply to the Native Color Dialog on the Mac
    OS X platform. If you still require this function, use the
    QColorDialog::DontUseNativeDialog option.
*/
void QColorDialog::setStandardColor(int index, QColor color)
{
    QColorDialogOptions::setStandardColor(index, color.rgba());
}

static inline void rgb2hsv(QRgb rgb, int &h, int &s, int &v)
{
    QColor c;
    c.setRgb(rgb);
    c.getHsv(&h, &s, &v);
}

namespace {

class QColorWell : public QWellArray
{
public:
    QColorWell(QWidget *parent, int r, int c, QRgb *vals)
        :QWellArray(r, c, parent), values(vals), mousePressed(false), oldCurrent(-1, -1)
    { setSizePolicy(QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum)); }

protected:
    void paintCellContents(QPainter *, int row, int col, const QRect&) Q_DECL_OVERRIDE;
    void mousePressEvent(QMouseEvent *e) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent *e) Q_DECL_OVERRIDE;
    void mouseReleaseEvent(QMouseEvent *e) Q_DECL_OVERRIDE;
#ifndef QT_NO_DRAGANDDROP
    void dragEnterEvent(QDragEnterEvent *e) Q_DECL_OVERRIDE;
    void dragLeaveEvent(QDragLeaveEvent *e) Q_DECL_OVERRIDE;
    void dragMoveEvent(QDragMoveEvent *e) Q_DECL_OVERRIDE;
    void dropEvent(QDropEvent *e) Q_DECL_OVERRIDE;
#endif

private:
    QRgb *values;
    bool mousePressed;
    QPoint pressPos;
    QPoint oldCurrent;

};

void QColorWell::paintCellContents(QPainter *p, int row, int col, const QRect &r)
{
    int i = row + col*numRows();
    p->fillRect(r, QColor(values[i]));
}

void QColorWell::mousePressEvent(QMouseEvent *e)
{
    oldCurrent = QPoint(selectedRow(), selectedColumn());
    QWellArray::mousePressEvent(e);
    mousePressed = true;
    pressPos = e->pos();
}

void QColorWell::mouseMoveEvent(QMouseEvent *e)
{
    QWellArray::mouseMoveEvent(e);
#ifndef QT_NO_DRAGANDDROP
    if (!mousePressed)
        return;
    if ((pressPos - e->pos()).manhattanLength() > QApplication::startDragDistance()) {
        setCurrent(oldCurrent.x(), oldCurrent.y());
        int i = rowAt(pressPos.y()) + columnAt(pressPos.x()) * numRows();
        QColor col(values[i]);
        QMimeData *mime = new QMimeData;
        mime->setColorData(col);
        QPixmap pix(cellWidth(), cellHeight());
        pix.fill(col);
        QPainter p(&pix);
        p.drawRect(0, 0, pix.width() - 1, pix.height() - 1);
        p.end();
        QDrag *drg = new QDrag(this);
        drg->setMimeData(mime);
        drg->setPixmap(pix);
        mousePressed = false;
        drg->start();
    }
#endif
}

#ifndef QT_NO_DRAGANDDROP
void QColorWell::dragEnterEvent(QDragEnterEvent *e)
{
    if (qvariant_cast<QColor>(e->mimeData()->colorData()).isValid())
        e->accept();
    else
        e->ignore();
}

void QColorWell::dragLeaveEvent(QDragLeaveEvent *)
{
    if (hasFocus())
        parentWidget()->setFocus();
}

void QColorWell::dragMoveEvent(QDragMoveEvent *e)
{
    if (qvariant_cast<QColor>(e->mimeData()->colorData()).isValid()) {
        setCurrent(rowAt(e->pos().y()), columnAt(e->pos().x()));
        e->accept();
    } else {
        e->ignore();
    }
}

void QColorWell::dropEvent(QDropEvent *e)
{
    QColor col = qvariant_cast<QColor>(e->mimeData()->colorData());
    if (col.isValid()) {
        int i = rowAt(e->pos().y()) + columnAt(e->pos().x()) * numRows();
        values[i] = col.rgb();
        update();
        e->accept();
    } else {
        e->ignore();
    }
}

#endif // QT_NO_DRAGANDDROP

void QColorWell::mouseReleaseEvent(QMouseEvent *e)
{
    if (!mousePressed)
        return;
    QWellArray::mouseReleaseEvent(e);
    mousePressed = false;
}

class QColorPicker : public QFrame
{
    Q_OBJECT
public:
    QColorPicker(QWidget* parent);
    ~QColorPicker();

    void setCrossVisible(bool visible);
public slots:
    void setCol(int h, int s);

signals:
    void newCol(int h, int s);

protected:
    QSize sizeHint() const Q_DECL_OVERRIDE;
    void paintEvent(QPaintEvent*) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent *) Q_DECL_OVERRIDE;
    void mousePressEvent(QMouseEvent *) Q_DECL_OVERRIDE;
    void resizeEvent(QResizeEvent *) Q_DECL_OVERRIDE;

private:
    int hue;
    int sat;

    QPoint colPt();
    int huePt(const QPoint &pt);
    int satPt(const QPoint &pt);
    void setCol(const QPoint &pt);

    QPixmap pix;
    bool crossVisible;
};

static int pWidth = 220;
static int pHeight = 200;

class QColorLuminancePicker : public QWidget
{
    Q_OBJECT
public:
    QColorLuminancePicker(QWidget* parent=0);
    ~QColorLuminancePicker();

public slots:
    void setCol(int h, int s, int v);
    void setCol(int h, int s);

signals:
    void newHsv(int h, int s, int v);

protected:
    void paintEvent(QPaintEvent*) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent *) Q_DECL_OVERRIDE;
    void mousePressEvent(QMouseEvent *) Q_DECL_OVERRIDE;

private:
    enum { foff = 3, coff = 4 }; //frame and contents offset
    int val;
    int hue;
    int sat;

    int y2val(int y);
    int val2y(int val);
    void setVal(int v);

    QPixmap *pix;
};


int QColorLuminancePicker::y2val(int y)
{
    int d = height() - 2*coff - 1;
    return 255 - (y - coff)*255/d;
}

int QColorLuminancePicker::val2y(int v)
{
    int d = height() - 2*coff - 1;
    return coff + (255-v)*d/255;
}

QColorLuminancePicker::QColorLuminancePicker(QWidget* parent)
    :QWidget(parent)
{
    hue = 100; val = 100; sat = 100;
    pix = 0;
    //    setAttribute(WA_NoErase, true);
}

QColorLuminancePicker::~QColorLuminancePicker()
{
    delete pix;
}

void QColorLuminancePicker::mouseMoveEvent(QMouseEvent *m)
{
    setVal(y2val(m->y()));
}
void QColorLuminancePicker::mousePressEvent(QMouseEvent *m)
{
    setVal(y2val(m->y()));
}

void QColorLuminancePicker::setVal(int v)
{
    if (val == v)
        return;
    val = qMax(0, qMin(v,255));
    delete pix; pix=0;
    repaint();
    emit newHsv(hue, sat, val);
}

//receives from a hue,sat chooser and relays.
void QColorLuminancePicker::setCol(int h, int s)
{
    setCol(h, s, val);
    emit newHsv(h, s, val);
}

void QColorLuminancePicker::paintEvent(QPaintEvent *)
{
    int w = width() - 5;

    QRect r(0, foff, w, height() - 2*foff);
    int wi = r.width() - 2;
    int hi = r.height() - 2;
    if (!pix || pix->height() != hi || pix->width() != wi) {
        delete pix;
        QImage img(wi, hi, QImage::Format_RGB32);
        int y;
        uint *pixel = (uint *) img.scanLine(0);
        for (y = 0; y < hi; y++) {
            uint *end = pixel + wi;
            std::fill(pixel, end, QColor::fromHsv(hue, sat, y2val(y + coff)).rgb());
            pixel = end;
        }
        pix = new QPixmap(QPixmap::fromImage(img));
    }
    QPainter p(this);
    p.drawPixmap(1, coff, *pix);
    const QPalette &g = palette();
    qDrawShadePanel(&p, r, g, true);
    p.setPen(g.foreground().color());
    p.setBrush(g.foreground());
    QPolygon a;
    int y = val2y(val);
    a.setPoints(3, w, y, w+5, y+5, w+5, y-5);
    p.eraseRect(w, 0, 5, height());
    p.drawPolygon(a);
}

void QColorLuminancePicker::setCol(int h, int s , int v)
{
    val = v;
    hue = h;
    sat = s;
    delete pix; pix=0;
    repaint();
}

QPoint QColorPicker::colPt()
{
    QRect r = contentsRect();
    return QPoint((360 - hue) * (r.width() - 1) / 360, (255 - sat) * (r.height() - 1) / 255);
}

int QColorPicker::huePt(const QPoint &pt)
{
    QRect r = contentsRect();
    return 360 - pt.x() * 360 / (r.width() - 1);
}

int QColorPicker::satPt(const QPoint &pt)
{
    QRect r = contentsRect();
    return 255 - pt.y() * 255 / (r.height() - 1);
}

void QColorPicker::setCol(const QPoint &pt)
{
    setCol(huePt(pt), satPt(pt));
}

QColorPicker::QColorPicker(QWidget* parent)
    : QFrame(parent)
    , crossVisible(true)
{
    hue = 0; sat = 0;
    setCol(150, 255);

    setAttribute(Qt::WA_NoSystemBackground);
    setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed) );
}

QColorPicker::~QColorPicker()
{
}

void QColorPicker::setCrossVisible(bool visible)
{
    if (crossVisible != visible) {
        crossVisible = visible;
        update();
    }
}

QSize QColorPicker::sizeHint() const
{
    return QSize(pWidth + 2*frameWidth(), pHeight + 2*frameWidth());
}

void QColorPicker::setCol(int h, int s)
{
    int nhue = qMin(qMax(0,h), 359);
    int nsat = qMin(qMax(0,s), 255);
    if (nhue == hue && nsat == sat)
        return;

    QRect r(colPt(), QSize(20,20));
    hue = nhue; sat = nsat;
    r = r.united(QRect(colPt(), QSize(20,20)));
    r.translate(contentsRect().x()-9, contentsRect().y()-9);
    //    update(r);
    repaint(r);
}

void QColorPicker::mouseMoveEvent(QMouseEvent *m)
{
    QPoint p = m->pos() - contentsRect().topLeft();
    setCol(p);
    emit newCol(hue, sat);
}

void QColorPicker::mousePressEvent(QMouseEvent *m)
{
    QPoint p = m->pos() - contentsRect().topLeft();
    setCol(p);
    emit newCol(hue, sat);
}

void QColorPicker::paintEvent(QPaintEvent* )
{
    QPainter p(this);
    drawFrame(&p);
    QRect r = contentsRect();

    p.drawPixmap(r.topLeft(), pix);

    if (crossVisible) {
        QPoint pt = colPt() + r.topLeft();
        p.setPen(Qt::black);
        p.fillRect(pt.x()-9, pt.y(), 20, 2, Qt::black);
        p.fillRect(pt.x(), pt.y()-9, 2, 20, Qt::black);
    }
}

void QColorPicker::resizeEvent(QResizeEvent *ev)
{
    QFrame::resizeEvent(ev);

    int w = width() - frameWidth() * 2;
    int h = height() - frameWidth() * 2;
    QImage img(w, h, QImage::Format_RGB32);
    int x, y;
    uint *pixel = (uint *) img.scanLine(0);
    for (y = 0; y < h; y++) {
        const uint *end = pixel + w;
        x = 0;
        while (pixel < end) {
            QPoint p(x, y);
            QColor c;
            c.setHsv(huePt(p), satPt(p), 200);
            *pixel = c.rgb();
            ++pixel;
            ++x;
        }
    }
    pix = QPixmap::fromImage(img);
}


class QColSpinBox : public QSpinBox
{
public:
    QColSpinBox(QWidget *parent)
        : QSpinBox(parent) { setRange(0, 255); }
    void setValue(int i) {
        const QSignalBlocker blocker(this);
        QSpinBox::setValue(i);
    }
};

class QColorShowLabel;

class QColorShower : public QWidget
{
    Q_OBJECT
public:
    QColorShower(QColorDialog *parent);

    //things that don't emit signals
    void setHsv(int h, int s, int v);

    int currentAlpha() const
    { return (colorDialog->options() & QColorDialog::ShowAlphaChannel) ? alphaEd->value() : 255; }
    void setCurrentAlpha(int a) { alphaEd->setValue(a); rgbEd(); }
    void showAlpha(bool b);
    bool isAlphaVisible() const;

    QRgb currentColor() const { return curCol; }
    QColor currentQColor() const { return curQColor; }
    void retranslateStrings();
    void updateQColor();

public slots:
    void setRgb(QRgb rgb);

signals:
    void newCol(QRgb rgb);
    void currentColorChanged(const QColor &color);

private slots:
    void rgbEd();
    void hsvEd();
    void htmlEd();

private:
    void showCurrentColor();
    int hue, sat, val;
    QRgb curCol;
    QColor curQColor;
    QLabel *lblHue;
    QLabel *lblSat;
    QLabel *lblVal;
    QLabel *lblRed;
    QLabel *lblGreen;
    QLabel *lblBlue;
    QLabel *lblHtml;
    QColSpinBox *hEd;
    QColSpinBox *sEd;
    QColSpinBox *vEd;
    QColSpinBox *rEd;
    QColSpinBox *gEd;
    QColSpinBox *bEd;
    QColSpinBox *alphaEd;
    QLabel *alphaLab;
    QLineEdit *htEd;
    QColorShowLabel *lab;
    bool rgbOriginal;
    QColorDialog *colorDialog;
    QGridLayout *gl;

    friend class QT_PREPEND_NAMESPACE(QColorDialog);
    friend class QT_PREPEND_NAMESPACE(QColorDialogPrivate);
};

class QColorShowLabel : public QFrame
{
    Q_OBJECT

public:
    QColorShowLabel(QWidget *parent) : QFrame(parent) {
        setFrameStyle(QFrame::Panel|QFrame::Sunken);
        setAcceptDrops(true);
        mousePressed = false;
    }
    void setColor(QColor c) { col = c; }

signals:
    void colorDropped(QRgb);

protected:
    void paintEvent(QPaintEvent *) Q_DECL_OVERRIDE;
    void mousePressEvent(QMouseEvent *e) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent *e) Q_DECL_OVERRIDE;
    void mouseReleaseEvent(QMouseEvent *e) Q_DECL_OVERRIDE;
#ifndef QT_NO_DRAGANDDROP
    void dragEnterEvent(QDragEnterEvent *e) Q_DECL_OVERRIDE;
    void dragLeaveEvent(QDragLeaveEvent *e) Q_DECL_OVERRIDE;
    void dropEvent(QDropEvent *e) Q_DECL_OVERRIDE;
#endif

private:
    QColor col;
    bool mousePressed;
    QPoint pressPos;
};

void QColorShowLabel::paintEvent(QPaintEvent *e)
{
    QPainter p(this);
    drawFrame(&p);
    p.fillRect(contentsRect()&e->rect(), col);
}

void QColorShower::showAlpha(bool b)
{
    alphaLab->setVisible(b);
    alphaEd->setVisible(b);
}

inline bool QColorShower::isAlphaVisible() const
{
    return alphaLab->isVisible();
}

void QColorShowLabel::mousePressEvent(QMouseEvent *e)
{
    mousePressed = true;
    pressPos = e->pos();
}

void QColorShowLabel::mouseMoveEvent(QMouseEvent *e)
{
#ifdef QT_NO_DRAGANDDROP
    Q_UNUSED(e);
#else
    if (!mousePressed)
        return;
    if ((pressPos - e->pos()).manhattanLength() > QApplication::startDragDistance()) {
        QMimeData *mime = new QMimeData;
        mime->setColorData(col);
        QPixmap pix(30, 20);
        pix.fill(col);
        QPainter p(&pix);
        p.drawRect(0, 0, pix.width() - 1, pix.height() - 1);
        p.end();
        QDrag *drg = new QDrag(this);
        drg->setMimeData(mime);
        drg->setPixmap(pix);
        mousePressed = false;
        drg->start();
    }
#endif
}

#ifndef QT_NO_DRAGANDDROP
void QColorShowLabel::dragEnterEvent(QDragEnterEvent *e)
{
    if (qvariant_cast<QColor>(e->mimeData()->colorData()).isValid())
        e->accept();
    else
        e->ignore();
}

void QColorShowLabel::dragLeaveEvent(QDragLeaveEvent *)
{
}

void QColorShowLabel::dropEvent(QDropEvent *e)
{
    QColor color = qvariant_cast<QColor>(e->mimeData()->colorData());
    if (color.isValid()) {
        col = color;
        repaint();
        emit colorDropped(col.rgb());
        e->accept();
    } else {
        e->ignore();
    }
}
#endif // QT_NO_DRAGANDDROP

void QColorShowLabel::mouseReleaseEvent(QMouseEvent *)
{
    if (!mousePressed)
        return;
    mousePressed = false;
}

QColorShower::QColorShower(QColorDialog *parent)
    : QWidget(parent)
{
    colorDialog = parent;

    curCol = qRgb(255, 255, 255);
    curQColor = Qt::white;

    gl = new QGridLayout(this);
    gl->setMargin(gl->spacing());
    lab = new QColorShowLabel(this);

#ifndef Q_OS_WINCE
#ifdef QT_SMALL_COLORDIALOG
    lab->setMinimumHeight(60);
#endif
    lab->setMinimumWidth(60);
#else
    lab->setMinimumWidth(20);
#endif

// For QVGA screens only the comboboxes and color label are visible.
// For nHD screens only color and luminence pickers and color label are visible.
#if !defined(QT_SMALL_COLORDIALOG)
    gl->addWidget(lab, 0, 0, -1, 1);
#else
    gl->addWidget(lab, 0, 0, 1, -1);
#endif
    connect(lab, SIGNAL(colorDropped(QRgb)), this, SIGNAL(newCol(QRgb)));
    connect(lab, SIGNAL(colorDropped(QRgb)), this, SLOT(setRgb(QRgb)));

    hEd = new QColSpinBox(this);
    hEd->setRange(0, 359);
    lblHue = new QLabel(this);
#ifndef QT_NO_SHORTCUT
    lblHue->setBuddy(hEd);
#endif
    lblHue->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
#if !defined(QT_SMALL_COLORDIALOG)
    gl->addWidget(lblHue, 0, 1);
    gl->addWidget(hEd, 0, 2);
#else
    gl->addWidget(lblHue, 1, 0);
    gl->addWidget(hEd, 2, 0);
#endif

    sEd = new QColSpinBox(this);
    lblSat = new QLabel(this);
#ifndef QT_NO_SHORTCUT
    lblSat->setBuddy(sEd);
#endif
    lblSat->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
#if !defined(QT_SMALL_COLORDIALOG)
    gl->addWidget(lblSat, 1, 1);
    gl->addWidget(sEd, 1, 2);
#else
    gl->addWidget(lblSat, 1, 1);
    gl->addWidget(sEd, 2, 1);
#endif

    vEd = new QColSpinBox(this);
    lblVal = new QLabel(this);
#ifndef QT_NO_SHORTCUT
    lblVal->setBuddy(vEd);
#endif
    lblVal->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
#if !defined(QT_SMALL_COLORDIALOG)
    gl->addWidget(lblVal, 2, 1);
    gl->addWidget(vEd, 2, 2);
#else
    gl->addWidget(lblVal, 1, 2);
    gl->addWidget(vEd, 2, 2);
#endif

    rEd = new QColSpinBox(this);
    lblRed = new QLabel(this);
#ifndef QT_NO_SHORTCUT
    lblRed->setBuddy(rEd);
#endif
    lblRed->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
#if !defined(QT_SMALL_COLORDIALOG)
    gl->addWidget(lblRed, 0, 3);
    gl->addWidget(rEd, 0, 4);
#else
    gl->addWidget(lblRed, 3, 0);
    gl->addWidget(rEd, 4, 0);
#endif

    gEd = new QColSpinBox(this);
    lblGreen = new QLabel(this);
#ifndef QT_NO_SHORTCUT
    lblGreen->setBuddy(gEd);
#endif
    lblGreen->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
#if !defined(QT_SMALL_COLORDIALOG)
    gl->addWidget(lblGreen, 1, 3);
    gl->addWidget(gEd, 1, 4);
#else
    gl->addWidget(lblGreen, 3, 1);
    gl->addWidget(gEd, 4, 1);
#endif

    bEd = new QColSpinBox(this);
    lblBlue = new QLabel(this);
#ifndef QT_NO_SHORTCUT
    lblBlue->setBuddy(bEd);
#endif
    lblBlue->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
#if !defined(QT_SMALL_COLORDIALOG)
    gl->addWidget(lblBlue, 2, 3);
    gl->addWidget(bEd, 2, 4);
#else
    gl->addWidget(lblBlue, 3, 2);
    gl->addWidget(bEd, 4, 2);
#endif

    alphaEd = new QColSpinBox(this);
    alphaLab = new QLabel(this);
#ifndef QT_NO_SHORTCUT
    alphaLab->setBuddy(alphaEd);
#endif
    alphaLab->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
#if !defined(QT_SMALL_COLORDIALOG)
    gl->addWidget(alphaLab, 3, 1, 1, 3);
    gl->addWidget(alphaEd, 3, 4);
#else
    gl->addWidget(alphaLab, 1, 3, 3, 1);
    gl->addWidget(alphaEd, 4, 3);
#endif
    alphaEd->hide();
    alphaLab->hide();
    lblHtml = new QLabel(this);
    htEd = new QLineEdit(this);
#ifndef QT_NO_SHORTCUT
    lblHtml->setBuddy(htEd);
#endif

#if !defined(QT_NO_REGULAREXPRESSION)
    QRegularExpression regExp(QStringLiteral("#([A-Fa-f0-9]{6}|[A-Fa-f0-9]{3})"));
    QRegularExpressionValidator *validator = new QRegularExpressionValidator(regExp, this);
    htEd->setValidator(validator);
#elif !defined(QT_NO_REGEXP)
    QRegExp regExp(QStringLiteral("#([A-Fa-f0-9]{6}|[A-Fa-f0-9]{3})"));
    QRegExpValidator *validator = new QRegExpValidator(regExp, this);
    htEd->setValidator(validator);
#else
    htEd->setReadOnly(true);
#endif
    htEd->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);

    lblHtml->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
#if defined(QT_SMALL_COLORDIALOG)
    gl->addWidget(lblHtml, 5, 0);
    gl->addWidget(htEd, 5, 1, 1, /*colspan=*/ 2);
#else
    gl->addWidget(lblHtml, 5, 1);
    gl->addWidget(htEd, 5, 2, 1, /*colspan=*/ 3);
#endif

    connect(hEd, SIGNAL(valueChanged(int)), this, SLOT(hsvEd()));
    connect(sEd, SIGNAL(valueChanged(int)), this, SLOT(hsvEd()));
    connect(vEd, SIGNAL(valueChanged(int)), this, SLOT(hsvEd()));

    connect(rEd, SIGNAL(valueChanged(int)), this, SLOT(rgbEd()));
    connect(gEd, SIGNAL(valueChanged(int)), this, SLOT(rgbEd()));
    connect(bEd, SIGNAL(valueChanged(int)), this, SLOT(rgbEd()));
    connect(alphaEd, SIGNAL(valueChanged(int)), this, SLOT(rgbEd()));
    connect(htEd, SIGNAL(textEdited(QString)), this, SLOT(htmlEd()));

    retranslateStrings();
}

} // unnamed namespace

inline QRgb QColorDialogPrivate::currentColor() const { return cs->currentColor(); }
inline int QColorDialogPrivate::currentAlpha() const { return cs->currentAlpha(); }
inline void QColorDialogPrivate::setCurrentAlpha(int a) { cs->setCurrentAlpha(a); }
inline void QColorDialogPrivate::showAlpha(bool b) { cs->showAlpha(b); }
inline bool QColorDialogPrivate::isAlphaVisible() const { return cs->isAlphaVisible(); }

QColor QColorDialogPrivate::currentQColor() const
{
    if (nativeDialogInUse)
        return platformColorDialogHelper()->currentColor();
    return cs->currentQColor();
}

void QColorShower::showCurrentColor()
{
    lab->setColor(currentColor());
    lab->repaint();
}

void QColorShower::rgbEd()
{
    rgbOriginal = true;
    curCol = qRgba(rEd->value(), gEd->value(), bEd->value(), currentAlpha());

    rgb2hsv(currentColor(), hue, sat, val);

    hEd->setValue(hue);
    sEd->setValue(sat);
    vEd->setValue(val);

    htEd->setText(QColor(curCol).name());

    showCurrentColor();
    emit newCol(currentColor());
    updateQColor();
}

void QColorShower::hsvEd()
{
    rgbOriginal = false;
    hue = hEd->value();
    sat = sEd->value();
    val = vEd->value();

    QColor c;
    c.setHsv(hue, sat, val);
    curCol = c.rgb();

    rEd->setValue(qRed(currentColor()));
    gEd->setValue(qGreen(currentColor()));
    bEd->setValue(qBlue(currentColor()));

    htEd->setText(c.name());

    showCurrentColor();
    emit newCol(currentColor());
    updateQColor();
}

void QColorShower::htmlEd()
{
    QColor c;
    QString t = htEd->text();
    c.setNamedColor(t);
    if (!c.isValid())
        return;
    curCol = qRgba(c.red(), c.green(), c.blue(), currentAlpha());
    rgb2hsv(curCol, hue, sat, val);

    hEd->setValue(hue);
    sEd->setValue(sat);
    vEd->setValue(val);

    rEd->setValue(qRed(currentColor()));
    gEd->setValue(qGreen(currentColor()));
    bEd->setValue(qBlue(currentColor()));

    showCurrentColor();
    emit newCol(currentColor());
    updateQColor();
}

void QColorShower::setRgb(QRgb rgb)
{
    rgbOriginal = true;
    curCol = rgb;

    rgb2hsv(currentColor(), hue, sat, val);

    hEd->setValue(hue);
    sEd->setValue(sat);
    vEd->setValue(val);

    rEd->setValue(qRed(currentColor()));
    gEd->setValue(qGreen(currentColor()));
    bEd->setValue(qBlue(currentColor()));

    htEd->setText(QColor(rgb).name());

    showCurrentColor();
    updateQColor();
}

void QColorShower::setHsv(int h, int s, int v)
{
    if (h < -1 || (uint)s > 255 || (uint)v > 255)
        return;

    rgbOriginal = false;
    hue = h; val = v; sat = s;
    QColor c;
    c.setHsv(hue, sat, val);
    curCol = c.rgb();

    hEd->setValue(hue);
    sEd->setValue(sat);
    vEd->setValue(val);

    rEd->setValue(qRed(currentColor()));
    gEd->setValue(qGreen(currentColor()));
    bEd->setValue(qBlue(currentColor()));

    htEd->setText(c.name());

    showCurrentColor();
    updateQColor();
}

void QColorShower::retranslateStrings()
{
    lblHue->setText(QColorDialog::tr("Hu&e:"));
    lblSat->setText(QColorDialog::tr("&Sat:"));
    lblVal->setText(QColorDialog::tr("&Val:"));
    lblRed->setText(QColorDialog::tr("&Red:"));
    lblGreen->setText(QColorDialog::tr("&Green:"));
    lblBlue->setText(QColorDialog::tr("Bl&ue:"));
    alphaLab->setText(QColorDialog::tr("A&lpha channel:"));
    lblHtml->setText(QColorDialog::tr("&HTML:"));
}

void QColorShower::updateQColor()
{
    QColor oldQColor(curQColor);
    curQColor.setRgba(qRgba(qRed(curCol), qGreen(curCol), qBlue(curCol), currentAlpha()));
    if (curQColor != oldQColor)
        emit currentColorChanged(curQColor);
}

//sets all widgets to display h,s,v
void QColorDialogPrivate::_q_newHsv(int h, int s, int v)
{
    if (!nativeDialogInUse) {
        cs->setHsv(h, s, v);
        cp->setCol(h, s);
        lp->setCol(h, s, v);
    }
}

//sets all widgets to display rgb
void QColorDialogPrivate::setCurrentRgbColor(QRgb rgb)
{
    if (!nativeDialogInUse) {
        cs->setRgb(rgb);
        _q_newColorTypedIn(rgb);
    }
}

// hack; doesn't keep curCol in sync, so use with care
void QColorDialogPrivate::setCurrentQColor(const QColor &color)
{
    Q_Q(QColorDialog);
    if (cs->curQColor != color) {
        cs->curQColor = color;
        emit q->currentColorChanged(color);
    }
}

// size of standard and custom color selector
enum {
    colorColumns = 8,
    standardColorRows = 6,
    customColorRows = 2
};

bool QColorDialogPrivate::selectColor(const QColor &col)
{
    QRgb color = col.rgb();
    // Check standard colors
    if (standard) {
        const QRgb *standardColors = QColorDialogOptions::standardColors();
        const QRgb *standardColorsEnd = standardColors + standardColorRows * colorColumns;
        const QRgb *match = std::find(standardColors, standardColorsEnd, color);
        if (match != standardColorsEnd) {
            const int index = int(match - standardColors);
            const int column = index / standardColorRows;
            const int row = index % standardColorRows;
            _q_newStandard(row, column);
            standard->setCurrent(row, column);
            standard->setSelected(row, column);
            standard->setFocus();
            return true;
        }
    }
    // Check custom colors
    if (custom) {
        const QRgb *customColors = QColorDialogOptions::customColors();
        const QRgb *customColorsEnd = customColors + customColorRows * colorColumns;
        const QRgb *match = std::find(customColors, customColorsEnd, color);
        if (match != customColorsEnd) {
            const int index = int(match - customColors);
            const int column = index / customColorRows;
            const int row = index % customColorRows;
            _q_newCustom(row, column);
            custom->setCurrent(row, column);
            custom->setSelected(row, column);
            custom->setFocus();
            return true;
        }
    }
    return false;
}

QColor QColorDialogPrivate::grabScreenColor(const QPoint &p)
{
    const QDesktopWidget *desktop = QApplication::desktop();
    const QPixmap pixmap = QGuiApplication::screens().at(desktop->screenNumber())->grabWindow(desktop->winId(),
                                                                                              p.x(), p.y(), 1, 1);
    QImage i = pixmap.toImage();
    return i.pixel(0, 0);
}

//sets all widgets except cs to display rgb
void QColorDialogPrivate::_q_newColorTypedIn(QRgb rgb)
{
    if (!nativeDialogInUse) {
        int h, s, v;
        rgb2hsv(rgb, h, s, v);
        cp->setCol(h, s);
        lp->setCol(h, s, v);
    }
}

void QColorDialogPrivate::_q_nextCustom(int r, int c)
{
    nextCust = r + customColorRows * c;
}

void QColorDialogPrivate::_q_newCustom(int r, int c)
{
    const int i = r + customColorRows * c;
    setCurrentRgbColor(QColorDialogOptions::customColor(i));
    if (standard)
        standard->setSelected(-1,-1);
}

void QColorDialogPrivate::_q_newStandard(int r, int c)
{
    setCurrentRgbColor(QColorDialogOptions::standardColor(r + c * 6));
    if (custom)
        custom->setSelected(-1,-1);
}

void QColorDialogPrivate::_q_pickScreenColor()
{
    Q_Q(QColorDialog);
    if (!colorPickingEventFilter)
        colorPickingEventFilter = new QColorPickingEventFilter(this);
    q->installEventFilter(colorPickingEventFilter);
    // If user pushes Escape, the last color before picking will be restored.
    beforeScreenColorPicking = cs->currentColor();
#ifndef QT_NO_CURSOR
    q->grabMouse(Qt::CrossCursor);
#else
    q->grabMouse();
#endif

#ifdef Q_OS_WIN32 // excludes WinCE and WinRT
    // On Windows mouse tracking doesn't work over other processes's windows
    updateTimer->start(30);

    // HACK: Because mouse grabbing doesn't work across processes, we have to have a dummy,
    // invisible window to catch the mouse click, otherwise we will click whatever we clicked
    // and loose focus.
    dummyTransparentWindow.show();
#endif
    q->grabKeyboard();
    /* With setMouseTracking(true) the desired color can be more precisely picked up,
     * and continuously pushing the mouse button is not necessary.
     */
    q->setMouseTracking(true);

    addCusBt->setDisabled(true);
    buttons->setDisabled(true);
    screenColorPickerButton->setDisabled(true);

    const QPoint globalPos = QCursor::pos();
    q->setCurrentColor(grabScreenColor(globalPos));
    updateColorLabelText(globalPos);
}

void QColorDialogPrivate::updateColorLabelText(const QPoint &globalPos)
{
    lblScreenColorInfo->setText(QColorDialog::tr("Cursor at %1, %2\nPress ESC to cancel")
                                .arg(globalPos.x())
                                .arg(globalPos.y()));
}

void QColorDialogPrivate::releaseColorPicking()
{
    Q_Q(QColorDialog);
    cp->setCrossVisible(true);
    q->removeEventFilter(colorPickingEventFilter);
    q->releaseMouse();
#ifdef Q_OS_WIN32
    updateTimer->stop();
    dummyTransparentWindow.setVisible(false);
#endif
    q->releaseKeyboard();
    q->setMouseTracking(false);
    lblScreenColorInfo->setText(QLatin1String("\n"));
    addCusBt->setDisabled(false);
    buttons->setDisabled(false);
    screenColorPickerButton->setDisabled(false);
}

void QColorDialogPrivate::init(const QColor &initial)
{
    Q_Q(QColorDialog);

    q->setSizeGripEnabled(false);
    q->setWindowTitle(QColorDialog::tr("Select Color"));

    // default: use the native dialog if possible.  Can be overridden in setOptions()
    nativeDialogInUse = (platformColorDialogHelper() != 0);
    colorPickingEventFilter = 0;
    nextCust = 0;

    if (!nativeDialogInUse)
        initWidgets();

#ifdef Q_OS_WIN32
    dummyTransparentWindow.resize(1, 1);
    dummyTransparentWindow.setFlags(Qt::Tool | Qt::FramelessWindowHint);
#endif

    q->setCurrentColor(initial);
}

void QColorDialogPrivate::initWidgets()
{
    Q_Q(QColorDialog);
    QVBoxLayout *mainLay = new QVBoxLayout(q);
    // there's nothing in this dialog that benefits from sizing up
    mainLay->setSizeConstraint(QLayout::SetFixedSize);

    QHBoxLayout *topLay = new QHBoxLayout();
    mainLay->addLayout(topLay);

    leftLay = 0;

#if defined(Q_OS_WINCE) || defined(QT_SMALL_COLORDIALOG)
    smallDisplay = true;
    const int lumSpace = 20;
#else
    // small displays (e.g. PDAs) cannot fit the full color dialog,
    // so just use the color picker.
    smallDisplay = (QApplication::desktop()->width() < 480 || QApplication::desktop()->height() < 350);
    const int lumSpace = topLay->spacing() / 2;
#endif

    if (!smallDisplay) {
        leftLay = new QVBoxLayout;
        topLay->addLayout(leftLay);

        standard = new QColorWell(q, standardColorRows, colorColumns, QColorDialogOptions::standardColors());
        lblBasicColors = new QLabel(q);
#ifndef QT_NO_SHORTCUT
        lblBasicColors->setBuddy(standard);
#endif
        q->connect(standard, SIGNAL(selected(int,int)), SLOT(_q_newStandard(int,int)));
        leftLay->addWidget(lblBasicColors);
        leftLay->addWidget(standard);

#if !defined(Q_OS_WINCE) && !defined(QT_SMALL_COLORDIALOG)
        // The screen color picker button
        screenColorPickerButton = new QPushButton();
        leftLay->addWidget(screenColorPickerButton);
        lblScreenColorInfo = new QLabel(QLatin1String("\n"));
        leftLay->addWidget(lblScreenColorInfo);
        q->connect(screenColorPickerButton, SIGNAL(clicked()), SLOT(_q_pickScreenColor()));
#endif

#if !defined(Q_OS_WINCE)
        leftLay->addStretch();
#endif

        custom = new QColorWell(q, customColorRows, colorColumns, QColorDialogOptions::customColors());
        custom->setAcceptDrops(true);

        q->connect(custom, SIGNAL(selected(int,int)), SLOT(_q_newCustom(int,int)));
        q->connect(custom, SIGNAL(currentChanged(int,int)), SLOT(_q_nextCustom(int,int)));
        lblCustomColors = new QLabel(q);
#ifndef QT_NO_SHORTCUT
        lblCustomColors->setBuddy(custom);
#endif
        leftLay->addWidget(lblCustomColors);
        leftLay->addWidget(custom);

        addCusBt = new QPushButton(q);
        QObject::connect(addCusBt, SIGNAL(clicked()), q, SLOT(_q_addCustom()));
        leftLay->addWidget(addCusBt);
    } else {
        // better color picker size for small displays
#if defined(QT_SMALL_COLORDIALOG)
        QSize screenSize = QApplication::desktop()->availableGeometry(QCursor::pos()).size();
        pWidth = pHeight = qMin(screenSize.width(), screenSize.height());
        pHeight -= 20;
        if(screenSize.height() > screenSize.width())
            pWidth -= 20;
#else
        pWidth = 150;
        pHeight = 100;
#endif
        custom = 0;
        standard = 0;
    }

    QVBoxLayout *rightLay = new QVBoxLayout;
    topLay->addLayout(rightLay);

    QHBoxLayout *pickLay = new QHBoxLayout;
    rightLay->addLayout(pickLay);

    QVBoxLayout *cLay = new QVBoxLayout;
    pickLay->addLayout(cLay);
    cp = new QColorPicker(q);

    cp->setFrameStyle(QFrame::Panel + QFrame::Sunken);

#if defined(QT_SMALL_COLORDIALOG)
    cp->hide();
#else
    cLay->addSpacing(lumSpace);
    cLay->addWidget(cp);
#endif
    cLay->addSpacing(lumSpace);

    lp = new QColorLuminancePicker(q);
#if defined(QT_SMALL_COLORDIALOG)
    lp->hide();
#else
    lp->setFixedWidth(20);
    pickLay->addSpacing(10);
    pickLay->addWidget(lp);
    pickLay->addStretch();
#endif

    QObject::connect(cp, SIGNAL(newCol(int,int)), lp, SLOT(setCol(int,int)));
    QObject::connect(lp, SIGNAL(newHsv(int,int,int)), q, SLOT(_q_newHsv(int,int,int)));

    rightLay->addStretch();

    cs = new QColorShower(q);
    pickLay->setMargin(cs->gl->margin());
    QObject::connect(cs, SIGNAL(newCol(QRgb)), q, SLOT(_q_newColorTypedIn(QRgb)));
    QObject::connect(cs, SIGNAL(currentColorChanged(QColor)),
                     q, SIGNAL(currentColorChanged(QColor)));
#if defined(QT_SMALL_COLORDIALOG)
    topLay->addWidget(cs);
#else
    rightLay->addWidget(cs);
    if (leftLay)
        leftLay->addSpacing(cs->gl->margin());
#endif

    buttons = new QDialogButtonBox(q);
    mainLay->addWidget(buttons);

    ok = buttons->addButton(QDialogButtonBox::Ok);
    QObject::connect(ok, SIGNAL(clicked()), q, SLOT(accept()));
    ok->setDefault(true);
    cancel = buttons->addButton(QDialogButtonBox::Cancel);
    QObject::connect(cancel, SIGNAL(clicked()), q, SLOT(reject()));

#ifdef Q_OS_WIN32
    updateTimer = new QTimer(q);
    QObject::connect(updateTimer, SIGNAL(timeout()), q, SLOT(_q_updateColorPicking()));
#endif
    retranslateStrings();
}

void QColorDialogPrivate::initHelper(QPlatformDialogHelper *h)
{
    QColorDialog *d = q_func();
    QObject::connect(h, SIGNAL(currentColorChanged(QColor)), d, SIGNAL(currentColorChanged(QColor)));
    QObject::connect(h, SIGNAL(colorSelected(QColor)), d, SIGNAL(colorSelected(QColor)));
    static_cast<QPlatformColorDialogHelper *>(h)->setOptions(options);
}

void QColorDialogPrivate::helperPrepareShow(QPlatformDialogHelper *)
{
    options->setWindowTitle(q_func()->windowTitle());
}

void QColorDialogPrivate::_q_addCustom()
{
    QColorDialogOptions::setCustomColor(nextCust, cs->currentColor());
    if (custom)
        custom->update();
    nextCust = (nextCust+1) % 16;
}

void QColorDialogPrivate::retranslateStrings()
{
    if (!smallDisplay) {
        lblBasicColors->setText(QColorDialog::tr("&Basic colors"));
        lblCustomColors->setText(QColorDialog::tr("&Custom colors"));
        addCusBt->setText(QColorDialog::tr("&Add to Custom Colors"));
        screenColorPickerButton->setText(QColorDialog::tr("&Pick Screen Color"));
    }

    cs->retranslateStrings();
}

bool QColorDialogPrivate::canBeNativeDialog() const
{
    Q_Q(const QColorDialog);
    if (nativeDialogInUse)
        return true;
    if (QCoreApplication::testAttribute(Qt::AA_DontUseNativeDialogs)
        || q->testAttribute(Qt::WA_DontShowOnScreen)
        || (q->options() & QColorDialog::DontUseNativeDialog)) {
        return false;
    }

    QLatin1String staticName(QColorDialog::staticMetaObject.className());
    QLatin1String dynamicName(q->metaObject()->className());
    return (staticName == dynamicName);
}

static const Qt::WindowFlags DefaultWindowFlags =
        Qt::Dialog | Qt::WindowTitleHint | Qt::MSWindowsFixedSizeDialogHint
        | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint;

/*!
    \class QColorDialog
    \brief The QColorDialog class provides a dialog widget for specifying colors.

    \ingroup standard-dialogs
    \inmodule QtWidgets

    The color dialog's function is to allow users to choose colors.
    For example, you might use this in a drawing program to allow the
    user to set the brush color.

    The static functions provide modal color dialogs.
    \omit
    If you require a modeless dialog, use the QColorDialog constructor.
    \endomit

    The static getColor() function shows the dialog, and allows the user to
    specify a color. This function can also be used to let users choose a
    color with a level of transparency: pass the ShowAlphaChannel option as
    an additional argument.

    The user can store customCount() different custom colors. The
    custom colors are shared by all color dialogs, and remembered
    during the execution of the program. Use setCustomColor() to set
    the custom colors, and use customColor() to get them.

    When pressing the "Pick Screen Color" button, the cursor changes to a haircross
    and the colors on the screen are scanned. The user can pick up one by clicking
    the mouse or the Enter button. Pressing Escape restores the last color selected
    before entering this mode.

    The \l{dialogs/standarddialogs}{Standard Dialogs} example shows
    how to use QColorDialog as well as other built-in Qt dialogs.

    \image fusion-colordialog.png A color dialog in the Fusion widget style.

    \sa QColor, QFileDialog, QFontDialog, {Standard Dialogs Example}
*/

/*!
    \since 4.5

    Constructs a color dialog with the given \a parent.
*/
QColorDialog::QColorDialog(QWidget *parent)
    : QDialog(*new QColorDialogPrivate, parent, DefaultWindowFlags)
{
    Q_D(QColorDialog);
    d->init(Qt::white);
}

/*!
    \since 4.5

    Constructs a color dialog with the given \a parent and specified
    \a initial color.
*/
QColorDialog::QColorDialog(const QColor &initial, QWidget *parent)
    : QDialog(*new QColorDialogPrivate, parent, DefaultWindowFlags)
{
    Q_D(QColorDialog);
    d->init(initial);
}

void QColorDialogPrivate::setCurrentColor(const QColor &color,  SetColorMode setColorMode)
{
    if (nativeDialogInUse) {
        platformColorDialogHelper()->setCurrentColor(color);
        return;
    }

    if (setColorMode & ShowColor) {
        setCurrentRgbColor(color.rgb());
        setCurrentAlpha(color.alpha());
    }
    if (setColorMode & SelectColor)
        selectColor(color);
}

/*!
    \property QColorDialog::currentColor
    \brief the currently selected color in the dialog
*/

void QColorDialog::setCurrentColor(const QColor &color)
{
    Q_D(QColorDialog);
    d->setCurrentColor(color);
}

QColor QColorDialog::currentColor() const
{
    Q_D(const QColorDialog);
    return d->currentQColor();
}

/*!
    Returns the color that the user selected by clicking the \uicontrol{OK}
    or equivalent button.

    \note This color is not always the same as the color held by the
    \l currentColor property since the user can choose different colors
    before finally selecting the one to use.
*/
QColor QColorDialog::selectedColor() const
{
    Q_D(const QColorDialog);
    return d->selectedQColor;
}

/*!
    Sets the given \a option to be enabled if \a on is true;
    otherwise, clears the given \a option.

    \sa options, testOption()
*/
void QColorDialog::setOption(ColorDialogOption option, bool on)
{
    const QColorDialog::ColorDialogOptions previousOptions = options();
    if (!(previousOptions & option) != !on)
        setOptions(previousOptions ^ option);
}

/*!
    \since 4.5

    Returns \c true if the given \a option is enabled; otherwise, returns
    false.

    \sa options, setOption()
*/
bool QColorDialog::testOption(ColorDialogOption option) const
{
    Q_D(const QColorDialog);
    return d->options->testOption(static_cast<QColorDialogOptions::ColorDialogOption>(option));
}

/*!
    \property QColorDialog::options
    \brief the various options that affect the look and feel of the dialog

    By default, all options are disabled.

    Options should be set before showing the dialog. Setting them while the
    dialog is visible is not guaranteed to have an immediate effect on the
    dialog (depending on the option and on the platform).

    \sa setOption(), testOption()
*/
void QColorDialog::setOptions(ColorDialogOptions options)
{
    Q_D(QColorDialog);

    if (QColorDialog::options() == options)
        return;

    d->options->setOptions(QColorDialogOptions::ColorDialogOptions(int(options)));
    if ((options & DontUseNativeDialog) && d->nativeDialogInUse) {
        d->nativeDialogInUse = false;
        d->initWidgets();
    }
    if (!d->nativeDialogInUse) {
        d->buttons->setVisible(!(options & NoButtons));
        d->showAlpha(options & ShowAlphaChannel);
    }
}

QColorDialog::ColorDialogOptions QColorDialog::options() const
{
    Q_D(const QColorDialog);
    return QColorDialog::ColorDialogOptions(int(d->options->options()));
}

/*!
    \enum QColorDialog::ColorDialogOption

    \since 4.5

    This enum specifies various options that affect the look and feel
    of a color dialog.

    \value ShowAlphaChannel Allow the user to select the alpha component of a color.
    \value NoButtons Don't display \uicontrol{OK} and \uicontrol{Cancel} buttons. (Useful for "live dialogs".)
    \value DontUseNativeDialog  Use Qt's standard color dialog instead of the operating system
                                native color dialog.

    \sa options, setOption(), testOption(), windowModality()
*/

/*!
    \fn void QColorDialog::currentColorChanged(const QColor &color)

    This signal is emitted whenever the current color changes in the dialog.
    The current color is specified by \a color.

    \sa color, colorSelected()
*/

/*!
    \fn void QColorDialog::colorSelected(const QColor &color);

    This signal is emitted just after the user has clicked \uicontrol{OK} to
    select a color to use. The chosen color is specified by \a color.

    \sa color, currentColorChanged()
*/

/*!
    Changes the visibility of the dialog. If \a visible is true, the dialog
    is shown; otherwise, it is hidden.
*/
void QColorDialog::setVisible(bool visible)
{
    Q_D(QColorDialog);

    if (visible){
        if (testAttribute(Qt::WA_WState_ExplicitShowHide) && !testAttribute(Qt::WA_WState_Hidden))
            return;
    } else if (testAttribute(Qt::WA_WState_ExplicitShowHide) && testAttribute(Qt::WA_WState_Hidden))
        return;

    if (visible)
        d->selectedQColor = QColor();

    if (d->nativeDialogInUse) {
        d->setNativeDialogVisible(visible);
        // Set WA_DontShowOnScreen so that QDialog::setVisible(visible) below
        // updates the state correctly, but skips showing the non-native version:
        setAttribute(Qt::WA_DontShowOnScreen);
    } else {
        setAttribute(Qt::WA_DontShowOnScreen, false);
    }

    QDialog::setVisible(visible);
}

/*!
    \overload
    \since 4.5

    Opens the dialog and connects its colorSelected() signal to the slot specified
    by \a receiver and \a member.

    The signal will be disconnected from the slot when the dialog is closed.
*/
void QColorDialog::open(QObject *receiver, const char *member)
{
    Q_D(QColorDialog);
    connect(this, SIGNAL(colorSelected(QColor)), receiver, member);
    d->receiverToDisconnectOnClose = receiver;
    d->memberToDisconnectOnClose = member;
    QDialog::open();
}

/*!
    \since 4.5

    Pops up a modal color dialog with the given window \a title (or "Select Color" if none is
    specified), lets the user choose a color, and returns that color. The color is initially set
    to \a initial. The dialog is a child of \a parent. It returns an invalid (see
    QColor::isValid()) color if the user cancels the dialog.

    The \a options argument allows you to customize the dialog.
*/
QColor QColorDialog::getColor(const QColor &initial, QWidget *parent, const QString &title,
                              ColorDialogOptions options)
{
    QColorDialog dlg(parent);
    if (!title.isEmpty())
        dlg.setWindowTitle(title);
    dlg.setOptions(options);
    dlg.setCurrentColor(initial);
    dlg.exec();
    return dlg.selectedColor();
}

/*!
    \obsolete

    Pops up a modal color dialog to allow the user to choose a color
    and an alpha channel (transparency) value. The color+alpha is
    initially set to \a initial. The dialog is a child of \a parent.

    If \a ok is non-null, \e *\a ok is set to true if the user clicked
    \uicontrol{OK}, and to false if the user clicked Cancel.

    If the user clicks Cancel, the \a initial value is returned.

    Use QColorDialog::getColor() instead, passing the
    QColorDialog::ShowAlphaChannel option.
*/

QRgb QColorDialog::getRgba(QRgb initial, bool *ok, QWidget *parent)
{
    QColor color(getColor(QColor(initial), parent, QString(), ShowAlphaChannel));
    QRgb result = color.isValid() ? color.rgba() : initial;
    if (ok)
        *ok = color.isValid();
    return result;
}

/*!
    Destroys the color dialog.
*/

QColorDialog::~QColorDialog()
{

}

/*!
    \reimp
*/
void QColorDialog::changeEvent(QEvent *e)
{
    Q_D(QColorDialog);
    if (e->type() == QEvent::LanguageChange)
        d->retranslateStrings();
    QDialog::changeEvent(e);
}

void QColorDialogPrivate::_q_updateColorPicking()
{
#ifndef QT_NO_CURSOR
    Q_Q(QColorDialog);
    static QPoint lastGlobalPos;
    QPoint newGlobalPos = QCursor::pos();
    if (lastGlobalPos == newGlobalPos)
        return;
    lastGlobalPos = newGlobalPos;

    if (!q->rect().contains(q->mapFromGlobal(newGlobalPos))) { // Inside the dialog mouse tracking works, handleColorPickingMouseMove will be called
        updateColorPicking(newGlobalPos);
#ifdef Q_OS_WIN32
        dummyTransparentWindow.setPosition(newGlobalPos);
#endif
    }
#endif // ! QT_NO_CURSOR
}

void QColorDialogPrivate::updateColorPicking(const QPoint &globalPos)
{
    const QColor color = grabScreenColor(globalPos);
    // QTBUG-39792, do not change standard, custom color selectors while moving as
    // otherwise it is not possible to pre-select a custom cell for assignment.
    setCurrentColor(color, ShowColor);
    updateColorLabelText(globalPos);

}

bool QColorDialogPrivate::handleColorPickingMouseMove(QMouseEvent *e)
{
    // If the cross is visible the grabbed color will be black most of the times
    cp->setCrossVisible(!cp->geometry().contains(e->pos()));

    updateColorPicking(e->globalPos());
    return true;
}

bool QColorDialogPrivate::handleColorPickingMouseButtonRelease(QMouseEvent *e)
{
    setCurrentColor(grabScreenColor(e->globalPos()), SetColorAll);
    releaseColorPicking();
    return true;
}

bool QColorDialogPrivate::handleColorPickingKeyPress(QKeyEvent *e)
{
    Q_Q(QColorDialog);
    if (e->matches(QKeySequence::Cancel)) {
        releaseColorPicking();
        q->setCurrentColor(beforeScreenColorPicking);
    } else if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        q->setCurrentColor(grabScreenColor(QCursor::pos()));
        releaseColorPicking();
    }
    e->accept();
    return true;
}

/*!
  Closes the dialog and sets its result code to \a result. If this dialog
  is shown with exec(), done() causes the local event loop to finish,
  and exec() to return \a result.

  \sa QDialog::done()
*/
void QColorDialog::done(int result)
{
    Q_D(QColorDialog);
    if (result == Accepted) {
        d->selectedQColor = d->currentQColor();
        emit colorSelected(d->selectedQColor);
    } else {
        d->selectedQColor = QColor();
    }
    QDialog::done(result);
    if (d->receiverToDisconnectOnClose) {
        disconnect(this, SIGNAL(colorSelected(QColor)),
                   d->receiverToDisconnectOnClose, d->memberToDisconnectOnClose);
        d->receiverToDisconnectOnClose = 0;
    }
    d->memberToDisconnectOnClose.clear();
}

QT_END_NAMESPACE

#include "qcolordialog.moc"
#include "moc_qcolordialog.cpp"

#endif // QT_NO_COLORDIALOG


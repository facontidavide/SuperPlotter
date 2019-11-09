#include "curvelist_view.h"
#include <QApplication>
#include <QDrag>
#include <QMessageBox>
#include <QMimeData>
#include <QDebug>
#include <QScrollBar>

CurveTableView::CurveTableView(QWidget *parent)
    : QTableView(parent), _model( new QStandardItemModel(0,2, this) )
{
    setEditTriggers(NoEditTriggers);
    setDragEnabled(false);
    setDefaultDropAction(Qt::IgnoreAction);
    setDragDropOverwriteMode(false);
    setDragDropMode(NoDragDrop);
    viewport()->installEventFilter(this);
    setModel(_model);
    verticalHeader()->setVisible(false);
    horizontalHeader()->setVisible(false);
    horizontalHeader()->setStretchLastSection(true);
    horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setShowGrid(false);
}

void CurveTableView::addItem(const QString &item_name)
{
    if (_model->findItems(item_name).size() > 0)
    {
        return;
    }

    auto item = new SortedTableItem(item_name);
    QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    font.setPointSize(_point_size);
    item->setFont(font);
    const int row = model()->rowCount();
    _model->setRowCount(row + 1);
    _model->setItem(row, 0, item);

    auto val_cell = new QStandardItem("-");
    val_cell->setTextAlignment(Qt::AlignRight);
    val_cell->setFlags(Qt::NoItemFlags | Qt::ItemIsEnabled);
    font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSize(_point_size);
    val_cell->setFont(font);
    val_cell->setFlags(Qt::NoItemFlags);

    _model->setItem(row, 1, val_cell);
}

void CurveTableView::refreshColumns()
{
    sortByColumn(0, Qt::AscendingOrder);
    horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    // TODO emit updateFilter();
}

std::vector<std::string> CurveTableView::getNonHiddenSelectedRows()
{
    std::vector<std::string> non_hidden_list;

    for (const auto& selected_index : selectionModel()->selectedRows(0))
    {
        if (!isRowHidden(selected_index.row()))
        {
            auto item = _model->item(selected_index.row(), 0);
            non_hidden_list.push_back(item->text().toStdString());
        }
    }
    return non_hidden_list;
}

void CurveTableView::refreshFontSize()
{
    horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);

    if( _model->rowCount() ) {
        verticalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
        verticalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    }

    for (int row = 0; row < _model->rowCount(); row++)
    {
        for (int col = 0; col < 2; col++)
        {
            auto item = _model->item(row, col);
            auto font = item->font();
            font.setPointSize(_point_size);
            item->setFont(font);
        }
    }

    horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
}

bool CurveTableView::applyVisibilityFilter(CurvesView::FilterType type, const QString &search_string)
{
    bool updated = false;
    _hidden_count = 0;
    QRegExp regexp( search_string, Qt::CaseInsensitive, QRegExp::Wildcard );
    QRegExpValidator v(regexp, nullptr);

    QStringList spaced_items = search_string.split(' ');

    for (int row=0; row < model()->rowCount(); row++)
    {
        auto item = _model->item(row,0);
        QString name = item->text();
        int pos = 0;
        bool toHide = false;

        if( search_string.isEmpty() == false )
        {
            if( type == REGEX)
            {
                toHide = v.validate( name, pos ) != QValidator::Acceptable;
            }
            else if( type == CONTAINS)
            {
                for (const auto& item: spaced_items)
                {
                    if( name.contains(item, Qt::CaseInsensitive) == false )
                    {
                        toHide = true;
                        break;
                    }
                }
            }
        }
        if( toHide ){ _hidden_count++; }

        if( toHide != isRowHidden(row) ){
            updated = true;
        }

        setRowHidden(row, toHide );
    }
    return updated;
}

CurvesView::CurvesView()
{
}

bool CurvesView::eventFilterBase(QObject *object, QEvent *event)
{
    QAbstractItemView* table_widget =  qobject_cast<QAbstractItemView*>(object);

    if( qobject_cast<QScrollBar*>(object) )
    {
        return false;
    }

    auto obj = object;
    while ( obj && !table_widget )
    {
        obj = obj->parent();
        table_widget =  qobject_cast<QAbstractItemView*>(obj);
    }

    bool shift_modifier_pressed =
            (QGuiApplication::keyboardModifiers() == Qt::ShiftModifier);
    bool ctrl_modifier_pressed =
            (QGuiApplication::keyboardModifiers() == Qt::ControlModifier);

    if (event->type() == QEvent::MouseButtonPress )
    {
        qDebug() << object;

        QMouseEvent *mouse_event = static_cast<QMouseEvent *>(event);

        _dragging = false;
        _drag_start_pos = mouse_event->pos();

        if (mouse_event->button() == Qt::LeftButton)
        {
            _newX_modifier = false;
        }
        else if (mouse_event->button() == Qt::RightButton)
        {
            _newX_modifier = true;
        }
        else{
            return true;
        }
        return false;
    }
    else if (event->type() == QEvent::MouseMove)
    {
        QMouseEvent *mouse_event = static_cast<QMouseEvent *>(event);
        double distance_from_click =
            (mouse_event->pos() - _drag_start_pos).manhattanLength();

        if ((mouse_event->buttons() == Qt::LeftButton ||
             mouse_event->buttons() == Qt::RightButton) &&
            distance_from_click >= QApplication::startDragDistance() &&
            !_dragging)
        {
            _dragging = true;
            QDrag *drag = new QDrag(table_widget);
            QMimeData *mimeData = new QMimeData;

            QByteArray mdata;
            QDataStream stream(&mdata, QIODevice::WriteOnly);

            for (const auto &curve_name : getNonHiddenSelectedRows())
            {
                stream << QString::fromStdString(curve_name);
            }

            if (!_newX_modifier)
            {
                mimeData->setData("curveslist/add_curve", mdata);
            }
            else
            {
                if (getNonHiddenSelectedRows().size() != 2)
                {
                    if (getNonHiddenSelectedRows().size() >= 1)
                    {
                        QMessageBox::warning(
                            table_widget, "New in version 2.3+",
                            "To create a new XY curve, you must select two "
                            "timeseries and "
                            "drag&drop them using the RIGHT mouse button.",
                            QMessageBox::Ok);
                    }
                    return true;
                }
                mimeData->setData("curveslist/new_XY_axis", mdata);

                QPixmap cursor(QSize(160, 30));
                cursor.fill(Qt::transparent);

                QPainter painter;
                painter.begin(&cursor);
                painter.setPen(QColor(22, 22, 22));

                QString text("Create a XY curve");
                painter.setFont(QFont("Arial", 14));

                painter.setBackground(Qt::transparent);
                painter.setPen(table_widget->palette().foreground().color());
                painter.drawText(QRect(0, 0, 160, 30),
                                 Qt::AlignHCenter | Qt::AlignVCenter, text);
                painter.end();

                drag->setDragCursor(cursor, Qt::MoveAction);
            }

            drag->setMimeData(mimeData);
            drag->exec(Qt::CopyAction | Qt::MoveAction);
        }
        return true;
    }
    else if (event->type() == QEvent::Wheel)
    {
        QWheelEvent *wheel_event = dynamic_cast<QWheelEvent *>(event);
        int prev_size = _point_size;
        if (ctrl_modifier_pressed)
        {
            if (_point_size > 6 && wheel_event->delta() < 0)
            {
                _point_size--;
            }
            else if (_point_size < 14 && wheel_event->delta() > 0)
            {
                _point_size++;
            }
            if (_point_size != prev_size)
            {
                refreshFontSize();
                QSettings settings;
                settings.setValue("FilterableListWidget/table_point_size", _point_size);
            }
            return true;
        }
    }

    return false;
}

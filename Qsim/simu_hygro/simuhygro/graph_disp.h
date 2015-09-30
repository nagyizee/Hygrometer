#ifndef GRAPH_DISP_H
#define GRAPH_DISP_H

#include <QMainWindow>

#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QPixmap>

namespace Ui {
class graphw;
}

class graph_disp : public QMainWindow
{
    Q_OBJECT

public:
    explicit graph_disp(QWidget *parent = 0);
    ~graph_disp();

public slots:
    void insert_tick( int coord_az, int coord_el, int coord_tl, int coord_au );


private:

    void redraw_disp_content();
    void gmem_clean();
    void gmem_draw_grid(int i);
    void g_putpixel( int x, int y, int color );
    int  g_getpixel( int x, int y );


    Ui::graphw *ui;

    QGraphicsScene *scene;
    QGraphicsPixmapItem *G_item;

    uchar *gmem;        // graphic memory for display simulator
    int gr_w;
    int gr_h;

    int crt_x;
    int crt_time;

signals:
    
public slots:
    
};

#endif // GRAPH_DISP_H

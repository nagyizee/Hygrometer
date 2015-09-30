#include "graph_disp.h"
#include "ui_graph.h"


#define GCOL_BLACK          0x000000
#define GCOL_CENTER_LINE    0x305030
#define GCOL_TICK_MAJOR     0x502525
#define GCOL_TICK_MINOR     0x252525

#define GCOL_AZIMUTH        0x10ff00
#define GCOL_ELEVATION      0xe0ff25
#define GCOL_TRANSLATION    0x2525ff
#define GCOL_AUXILIARY      0x25ffee


graph_disp::graph_disp(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::graphw)
{
    ui->setupUi(this);

    gr_w    = ui->gView->width();
    gr_h    = ui->gView->height();
    crt_x   = 0;
    crt_time= 0;

    gmem    = (uchar*)malloc( gr_w * gr_h * 3 );
    scene   = new QGraphicsScene( this );

    QImage image( gmem, gr_w, gr_h, gr_w * 3, QImage::Format_RGB888 );
    image.fill( Qt::black );
    G_item  = new QGraphicsPixmapItem( QPixmap::fromImage( image ));
    scene->addItem(G_item);
    ui->gView->setScene(scene);

    gmem_clean();
    redraw_disp_content();


}


graph_disp::~graph_disp()
{
    delete ui;
    delete gmem;
}


void graph_disp::insert_tick( int coord_az, int coord_el, int coord_tl, int coord_au )
{
    // all are scaled to 1000, 500 is the middle line

    if ( crt_x == gr_w )
    {
        crt_x--;
        for (int i=0; i<gr_w-1; i++)
        {
            for (int j=0; j<gr_h; j++)
            {
                g_putpixel( i, j, g_getpixel(i+1, j) );
            }
        }
        gmem_draw_grid( gr_w-1 );
    }

    if ( crt_time == 50 )
    {
        crt_time = 0;
        for (int j=0; j< gr_h; j++)
        {
            g_putpixel( crt_x, j, GCOL_TICK_MINOR );
        }

    }

    g_putpixel( crt_x, gr_h - 1 - coord_az * gr_h / 100000, GCOL_AZIMUTH );
    g_putpixel( crt_x, gr_h - 1 - coord_el * gr_h / 100000, GCOL_ELEVATION );
    g_putpixel( crt_x, gr_h - 1 - coord_tl * gr_h / 100000, GCOL_TRANSLATION );
    g_putpixel( crt_x, gr_h - 1 - coord_au * gr_h / 100000, GCOL_AUXILIARY );

    redraw_disp_content();

    crt_x++;
    crt_time++;
}




void graph_disp::gmem_draw_grid(int i)
{
    int j;
    for (j=0; j<gr_h; j++)
        g_putpixel( i, j, GCOL_BLACK );

    g_putpixel( i, gr_h / 2, GCOL_CENTER_LINE );

    for (j=0; j<20; j++)
    {
        g_putpixel( i, (gr_h/2) + ((gr_h/2) * (j+1) / 20), GCOL_TICK_MINOR );
        g_putpixel( i, (gr_h/2) - ((gr_h/2) * (j+1) / 20), GCOL_TICK_MINOR );
    }

    for (j=0; j<4; j++)
    {
        g_putpixel( i, (gr_h/2) + ((gr_h/2) * (j+1) / 4), GCOL_TICK_MAJOR );
        g_putpixel( i, (gr_h/2) - ((gr_h/2) * (j+1) / 4), GCOL_TICK_MAJOR );
    }
}




void graph_disp::gmem_clean()
{
    int i;
    memset( gmem, 0, gr_w * gr_h * 3 );

    // draw center line
    for ( i=0; i<gr_w; i++ )
    {
        gmem_draw_grid(i);
    }

    crt_x = 0;
    crt_time = 0;

}


void graph_disp::g_putpixel( int x, int y, int color )
{
    if ( (x<0) || (y<0) || (x>= gr_w) || (y>= gr_h))
        return;

    unsigned char r;
    unsigned char g;
    unsigned char b;

    r = (color >> 16) & 0xff;
    g = (color >> 8 ) & 0xff;
    b = (color ) & 0xff;

    *( gmem + 3 * ( x + y * gr_w ) ) = r;
    *( gmem + 3 * ( x + y * gr_w ) + 1 ) = g;
    *( gmem + 3 * ( x + y * gr_w ) + 2 ) = b;
}


int  graph_disp::g_getpixel( int x, int y )
{
    unsigned char r;
    unsigned char g;
    unsigned char b;

    r = *( gmem + 3 * ( x + y * gr_w ) );
    g = *( gmem + 3 * ( x + y * gr_w ) + 1 );
    b = *( gmem + 3 * ( x + y * gr_w ) + 2 );

    return ( (r<<16) | (g<<8) | b );
}



void graph_disp::redraw_disp_content()
{
    QImage image( gmem, gr_w, gr_h, gr_w * 3, QImage::Format_RGB888 );

    scene->removeItem(G_item);
    delete G_item;
    G_item  = new QGraphicsPixmapItem( QPixmap::fromImage( image ));
    scene->addItem(G_item);
}


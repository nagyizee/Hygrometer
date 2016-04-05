#ifndef ON_QT_PLATFORM
  #include "stdlib_extension.h"
#endif
#include <string.h>
#include <stdlib.h>
#include "graphic_lib.h"
#include "ui_elements.h"


/********************************************************

    UI elements:
        - drop down menu
        - list
        - checkbox
        - numeric
        - edit box ( text/numeric + fp)
        - time 

    main routines:
        ui_element_display()    - renders an ui element to display
        ui_element_poll()       - poll routine with event input

        The poll routine processes the keypress and timing events and 
        refreshes the ui element in focus
        The following callbacks are valid:
                
                    val.ch  Ok.press  esc.long  esc.  edit.done 
        - list:       x        x        x        x        -
        - checkbox:   -        x        -        x        -
        - pbutton:    -        x        -        x        -
        - numeric:    x        x        x        x        -
        - edit box:   x        -        x        x        x
        - time:       x        -        x        x        x
        - menu        x        x        -        x        -


        the poll routine filters the key events - removing the internally
        captured key event.
        Keys which trigger a callback are removed also.
       




**********************************************************/





#define ELEM_EDIT_BLINK_TIME        25      // 250ms
#define ELEM_EDIT_LONGPRESS_TIME    50      // 500ms

uint32 int_focus = 0;       // internal focus selection

static inline void uiel_control_checkbox_toggle( struct Suiel_control_checkbox *handle );




static inline bool internal_control_numeric_OK( struct Suiel_control_numeric *handle )
{
    if ( handle->call_OK )
    {
        int val = handle->val;
        handle->call_OK( handle->ccont_OK, (void*)&val );
    }
    return false;
}

static inline bool internal_control_edit_OK( struct Suiel_control_edit *handle )
{
    int_focus ^= 0x80;
    return true;
}

static inline bool internal_control_time_OK( struct Suiel_control_time *handle )
{
    int_focus ^= 0x80;
    return true;
}

static inline bool internal_control_checkbox_OK( struct Suiel_control_checkbox *handle )
{
    uiel_control_checkbox_toggle( handle );
    if ( handle->call_OK )
    {
        bool val = (handle->set == 1);
        handle->call_OK( handle->ccont_OK, (void*)&val );
    }
    return true;
}

static inline bool internal_control_pbutton_OK( struct Suiel_control_pushbutton *handle )
{
    if ( handle->call_OK )
    {
        handle->call_OK( handle->ccont_OK, NULL );
    }
    return true;
}

static bool internal_control_list_OK( struct Suiel_control_list *handle )
{
    if ( handle->call_OK )
    {
        int val = handle->elem_crt;
        handle->call_OK( handle->ccont_OK, (void*)&val );
    }
    return false;
}

static inline bool internal_control_menu_OK( struct Suiel_dropdown_menu *handle )
{
    if ( handle->call_OK )
    {
        int val = handle->elem_crt;
        handle->call_OK( handle->ccont_OK, (void*)&val );
    }
    return false;
}



static inline bool internal_control_numeric_ESC( struct Suiel_control_numeric *handle )
{
    if ( handle->call_Esc )
    {
        int val = handle->val;
        handle->call_Esc( handle->ccont_Esc, (void*)&val );
    }
    return false;
}

static inline bool internal_control_edit_ESC( struct Suiel_control_edit *handle )
{

    // TBI
    return false;
}

static inline bool internal_control_time_ESC( struct Suiel_control_time *handle )
{

    // TBI
    return false;
}

static inline bool internal_control_checkbox_ESC( struct Suiel_control_checkbox *handle )
{
    if ( handle->call_Esc )
    {
        bool val = (handle->set == 1);
        handle->call_Esc( handle->ccont_Esc, (void*)&val );
    }
    return false;
}

static inline bool internal_control_pbutton_ESC( struct Suiel_control_pushbutton *handle )
{
    if ( handle->call_Esc )
    {
        handle->call_Esc( handle->ccont_Esc, NULL );
    }
    return false;
}

static inline bool internal_control_list_ESC( struct Suiel_control_list *handle )
{
    if ( handle->call_Esc )
    {
        int val = handle->elem_crt;
        handle->call_Esc( handle->ccont_Esc, (void*)&val );
    }
    return false;
}

static inline bool internal_control_menu_ESC( struct Suiel_dropdown_menu *handle )
{
    if ( handle->call_Esc )
    {
        int val = handle->elem_crt;
        handle->call_Esc( handle->ccont_Esc, (void*)&val );
    }
    return false;
}




static inline void internal_control_list_longEsc( struct Suiel_control_list *handle )
{
    if ( handle->call_EscLong )
    {
        int val = handle->elem_crt;
        handle->call_EscLong( handle->ccont_EscLong, (void*)&val );
    }
}

static inline void internal_control_edit_longEsc( struct Suiel_control_edit *handle )
{
    if ( handle->call_EscLong )
    {
        if ( handle->type == uiedit_numeric )
        {
            int val;
            val = uiel_control_edit_get_num(handle);
            handle->call_EscLong( handle->ccont_EscLong, (void*)&val );
        }
        else
            handle->call_EscLong( handle->ccont_EscLong, (void*)handle->content );
    }
}

static inline void internal_control_time_longEsc( struct Suiel_control_time *handle )
{
    if ( handle->call_EscLong )
    {
        handle->call_EscLong( handle->ccont_EscLong, (void*)&handle->time );
    }
}

static inline void internal_control_numeric_longEsc( struct Suiel_control_numeric *handle )
{
    if ( handle->call_EscLong )
        handle->call_EscLong( handle->ccont_EscLong, (void*)&handle->val );
}



// helper for sliding menu

static int uiel_internal_menu_find_index( char *labels, uint8 index )
{
    int i = 0;
    int idx = 0;

    do {
        if ( idx == index )
            return i;

        if ( *labels == 0x00 )
            idx++;

        i++;
        labels++;

    } while (i<UIEL_MENU_TOTAL_MAX);
    return -1;
}


static int uiel_internal_menu_insert( char *labels, char *element, uint8 *elem_total )
{
    int entry_poz;

    entry_poz = uiel_internal_menu_find_index( labels, *elem_total );
    if ( entry_poz < 0 )
        return entry_poz;

    if (strlen(element) > (UIEL_MENU_TOTAL_MAX - entry_poz - 1))
        return -1;
    if (*elem_total == UIEL_MENU_TOTAL_MAX)
        return -2;

    strcpy( labels + entry_poz, element );
    (*elem_total)++;

    return 0;
}


// DROPDOWN MENU

// inits or resets a dropdown menu
void uiel_dropdown_menu_init( struct Suiel_dropdown_menu *handle, int xmin, int xmax, int ymin, int ymax )
{
    memset( handle, 0, sizeof(*handle));

    handle->ID = ELEM_ID_DROPDOWN_MENU;
    handle->poz_x1 = xmin;
    handle->poz_x2 = xmax;
    handle->poz_y1 = ymin;
    handle->poz_y2 = ymax;

    grf_setup_font( uitxt_smallbold, 1, -1 );
    handle->ytext = Gtext_GetCharacterHeight() + 2;
    if (handle->ytext == 0 )    // prevent div/0
        handle->ytext = 1;
    handle->melem = ( ymax - ymin ) / handle->ytext;     // how many elements can be displayed in this menu window

    int_focus = 1;  // menu is always in selected focus
}

void uiel_dropdown_menu_set_callback( struct Suiel_dropdown_menu *handle, enum EUIelCallMenu select, int context, uiel_callback func )
{
    switch (select)
    {
        case UICmenu_OK:
            handle->call_OK = func;
            handle->ccont_OK = (uint8)context;
            break;
        case UICmenu_Esc:
            handle->call_Esc = func;
            handle->ccont_Esc = (uint8)context;
            break;
        case UICmenu_Vchange:
            handle->call_Vchange = func;
            handle->ccont_Vchange = (uint8)context;
            break;
    }
}

// add element to a dropdown menu
// returns the menu element index
int uiel_dropdown_menu_add_item( struct Suiel_dropdown_menu *handle, char *element )
{
    return uiel_internal_menu_insert( handle->labels, element, &handle->elem_total );
}

// display the sliding menu current status
static inline void uiel_dropdown_menu_display( struct Suiel_dropdown_menu *handle )
{
    char *crt_label;
    int i;
    int crt_poz;
    int ytext;
    int y;

    // clear the underlaying area
    Graphic_SetColor( 0 );
    Graphic_FillRectangle( handle->poz_x1, handle->poz_y1, handle->poz_x2, handle->poz_y2, 0 );

    // draw the scroll bar
    Graphic_SetColor( 1 );
    if ( handle->elem_total <= handle->melem )
    {
        Graphic_FillRectangle( handle->poz_x1, handle->poz_y1, handle->poz_x1 + 3, handle->poz_y2, -1 );
    }
    else
    {
        i = ((int)(handle->poz_y2 - handle->poz_y1) * handle->melem ) / (int)(handle->elem_total);     // height of the scroll indicator
        y = handle->poz_y1 + 1 + ( ((int)(handle->poz_y2 - handle->poz_y1 - i) * handle->elem_crt )/ handle->elem_total) ;
        Graphic_Rectangle( handle->poz_x1, handle->poz_y1, handle->poz_x1 + 3, handle->poz_y2 );
        Graphic_FillRectangle( handle->poz_x1+1, y, handle->poz_x1 + 2, y + i + 1, 1 );
    }

    // draw the content
    grf_setup_font( uitxt_smallbold, 1, -1 );
    ytext = handle->ytext;
    y = handle->poz_y1;

    Graphic_Window( handle->poz_x1, handle->poz_y1, handle->poz_x2, handle->poz_y2 );
    i = handle->moffs;
    while ( (y < handle->poz_y2) && (i < handle->elem_total) )
    {
        crt_poz = uiel_internal_menu_find_index( handle->labels, i );
        crt_label = handle->labels + crt_poz;
        Gtext_SetCoordinates( handle->poz_x1 + 6, y + 1 );
        Gtext_PutText( crt_label );
        y += ytext;
        i++;
    }
    Graphic_Window( 0, 0, GDISP_WIDTH-1, GDISP_HEIGHT-1 );

    // draw the selection bar
    i = (handle->elem_crt - handle->moffs) * ytext + handle->poz_y1;    // calculate the relative position of the selection bar
    Graphic_FillRectangle( handle->poz_x1 + 5, i, handle->poz_x2, i + ytext - 1, -1 );
}

// move to right in sliding menu
void uiel_dropdown_menu_set_next( struct Suiel_dropdown_menu *handle )
{
    handle->elem_crt++;
    if (handle->elem_crt >= handle->elem_total )
        handle->elem_crt--;

    if ( (handle->elem_crt - handle->moffs) >= ( handle->melem ) )
    {
        handle->moffs++;
    }

}

// move to left in sliding menu
void uiel_dropdown_menu_set_prew( struct Suiel_dropdown_menu *handle )
{
    if (handle->elem_crt > 0 )
    {
        handle->elem_crt--;

        if ( handle->elem_crt < handle->moffs )
        {
            handle->moffs--;
        }

        if ( handle->call_Vchange )
            handle->call_Vchange( handle->ccont_Vchange, (void*)&handle->elem_crt );
    }
}

// set the menu index
void uiel_dropdown_menu_set_index( struct Suiel_dropdown_menu *handle, unsigned int index )
{
    if ( index < handle->elem_total )
    {
        handle->elem_crt = index;
        if ( index < handle->melem )
            handle->moffs   = 0;
        else
            handle->moffs   = index - handle->melem + 1;

        if ( handle->call_Vchange )
            handle->call_Vchange( handle->ccont_Vchange, (void*)&handle->elem_crt );
    }

}

// get the current index
int uiel_dropdown_menu_get_index( struct Suiel_dropdown_menu *handle )
{
    return handle->elem_crt;
}



// LIST

void uiel_control_list_init( struct Suiel_control_list *handle, int xpoz, int ypoz, int width, enum Etextstyle style, int color, bool thin_border )
{
    memset( handle, 0, sizeof(*handle));

    handle->ID = ELEM_ID_LIST;
    handle->poz_x = xpoz;
    handle->poz_y = ypoz;
    handle->width = width;
    handle->textstyle = style;
    handle->color = color;
    handle->thin_border = thin_border;
    grf_setup_font( style, 0, 0 );
    if ( handle->thin_border )
        handle->height = Gtext_GetCharacterHeight();
    else
        handle->height = Gtext_GetCharacterHeight() + 2;

    if (handle->height == 0 )    // prevent div/0
        handle->height = 1;
}

void uiel_control_list_set_callback( struct Suiel_control_list *handle, enum EUIelCallList select, int context, uiel_callback func )
{
    switch (select)
    {
        case UIClist_OK:
            handle->call_OK = func;
            handle->ccont_OK = (uint8)context;
            break;
        case UIClist_Esc:
            handle->call_Esc = func;
            handle->ccont_Esc = (uint8)context;
            break;
        case UIClist_EscLong:
            handle->call_EscLong = func;
            handle->ccont_EscLong = (uint8)context;
            break;
        case UIClist_Vchange:
            handle->call_Vchange = func;
            handle->ccont_Vchange = (uint8)context;
            break;
    }
}

int uiel_control_list_add_item( struct Suiel_control_list *handle, const char *element, int value )
{
    int entry_poz;

    entry_poz = uiel_internal_menu_find_index( handle->labels, handle->elem_total );
    if ( entry_poz < 0 )
        return entry_poz;

    if (strlen(element) > (UIEL_LIST_LABEL_MAX - entry_poz - 1))
        return -1;
    if (handle->elem_total == UIEL_MENU_TOTAL_MAX)
        return -2;

    strcpy( handle->labels + entry_poz, element );
    value &= 0xffff;
    handle->value[handle->elem_total] = value; 
    (handle->elem_total)++;
    return 0;
}


void uiel_control_list_set_next( struct Suiel_control_list *handle )
{
    if (handle->elem_crt < (handle->elem_total - 1))
        handle->elem_crt++;

    if ( handle->call_Vchange )
    {
        int val = handle->elem_crt;
        handle->call_Vchange( handle->ccont_Vchange, (void*)&val );
    }
}


void uiel_control_list_set_prew( struct Suiel_control_list *handle )
{
    if (handle->elem_crt > 0 )
        handle->elem_crt--;

    if ( handle->call_Vchange )
    {
        int val = handle->elem_crt;
        handle->call_Vchange( handle->ccont_Vchange, (void*)&val );
    }
}

void uiel_control_list_set_index( struct Suiel_control_list *handle, unsigned int index )
{
    if ( index < handle->elem_total )
        handle->elem_crt = index;
}

int uiel_control_list_get_index( struct Suiel_control_list *handle )
{
    return handle->elem_crt;
}

int uiel_control_list_get_value( struct Suiel_control_list *handle )
{
    return handle->value[ handle->elem_crt ];
}

static inline void uiel_control_list_display( struct Suiel_control_list *handle, bool focus )
{
    char *crt_label;
    int crt_poz;
    int y;
    int xend;
    int yend;

    y = handle->poz_y;
    xend = handle->poz_x + handle->width;
    yend = handle->poz_y + handle->height;

    // clear the underlaying area
    Graphic_SetColor( 1 - handle->color );
    Graphic_FillRectangle( handle->poz_x, y, xend, yend, 1 - handle->color );

    // draw the content
    grf_setup_font( (enum Etextstyle)handle->textstyle, handle->color, -1 );

    Graphic_Window( handle->poz_x, y, xend, yend );
    crt_poz = uiel_internal_menu_find_index( handle->labels, handle->elem_crt );
    crt_label = handle->labels + crt_poz;
    if ( handle->thin_border )
        Gtext_SetCoordinates( handle->poz_x + 1, y+1 );
    else
        Gtext_SetCoordinates( handle->poz_x + 2, y+2 );

    Gtext_PutText( crt_label );
    Graphic_Window( 0, 0, GDISP_WIDTH-1, GDISP_HEIGHT-1 );

    if ( focus )
    {
        if ( (handle->ID & ELEM_IN_FOCUS) == 0 )
        {
            int_focus = 0;                  // the control just got the focus, reset internal focus state
            handle->ID |= ELEM_IN_FOCUS;    
        }

        if ( handle->thin_border )
        {
            Graphic_SetColor(-1);
            Graphic_FillRectangle( handle->poz_x, y, xend, yend, -1);
        }
        else
        {
            Graphic_SetColor(1);
            Graphic_Rectangle( handle->poz_x, y, xend, yend );
            Graphic_PutPixel( handle->poz_x, y, -1 );
            Graphic_PutPixel( xend, y, -1 );
            Graphic_PutPixel( handle->poz_x, yend, -1 );
            Graphic_PutPixel( xend, yend, -1 );
        }
    }
    else
    {
//        Graphic_SetColor(0);
//        Graphic_Rectangle( handle->poz_x, y, xend, yend );
        handle->ID &= ~ELEM_IN_FOCUS;
    }
}



/// CHECKBOX

void uiel_control_checkbox_init( struct Suiel_control_checkbox *handle, int xpoz, int ypoz )
{
    memset( handle, 0, sizeof(*handle));
    handle->ID = ELEM_ID_CHECKBOX;
    handle->xpoz = xpoz;
    handle->ypoz = ypoz;
    handle->set = 0;
}


void uiel_control_checkbox_set_callback( struct Suiel_control_checkbox *handle, enum EUIelCallCheckBox select, int context, uiel_callback func )
{
    switch (select)
    {
        case UICcb_OK:
            handle->call_OK = func;
            handle->ccont_OK = (uint8)context;
            break;
        case UICcb_Esc:
            handle->call_Esc = func;
            handle->ccont_Esc = (uint8)context;
            break;
    }
}


void uiel_control_checkbox_set( struct Suiel_control_checkbox *handle, bool set )
{
    if ( set )
        handle->set = 1;
    else
        handle->set = 0;
}

bool uiel_control_checkbox_get( struct Suiel_control_checkbox *handle )
{
    return (handle->set == 1);
}

static inline void uiel_control_checkbox_toggle( struct Suiel_control_checkbox *handle )
{
    handle->set ^= (uint8)0x01;
}


static inline void uiel_control_checkbox_display( struct Suiel_control_checkbox *handle, bool focus )
{
    Graphic_SetColor( 1 );
    Graphic_FillRectangle( handle->xpoz, handle->ypoz, handle->xpoz+9, handle->ypoz+9, 0 );
    if ( handle->set )
        uibm_put_bitmap( handle->xpoz + 1, handle->ypoz + 1, BMP_GRF_SELECTED );
    Graphic_PutPixel(  handle->xpoz, handle->ypoz, 0 );
    Graphic_PutPixel(  handle->xpoz+9, handle->ypoz, 0 );
    Graphic_PutPixel(  handle->xpoz, handle->ypoz+9, 0 );
    Graphic_PutPixel(  handle->xpoz+9, handle->ypoz+9, 0 );
    if ( focus )
    {
        if ( (handle->ID & ELEM_IN_FOCUS) == 0 )
        {
            int_focus = 0;  // the control just got the focus, reset the internal focus state
            handle->ID |= ELEM_IN_FOCUS;
        }

        Graphic_FillRectangle( handle->xpoz+1, handle->ypoz+1, handle->xpoz+8, handle->ypoz+8, -1 );
    }
    else
    {
        handle->ID &= ~ELEM_IN_FOCUS;
    }
}


/// PUSH BUTTON

void uiel_control_pushbutton_init( struct Suiel_control_pushbutton *handle, int xpoz, int ypoz, int width, int height )
{
    memset( handle, 0, sizeof(*handle));
    handle->ID = ELEM_ID_PUSHBUTTON;

    handle->xpoz = xpoz;
    handle->ypoz = ypoz;
    handle->w = width;
    handle->h = height;

}

void uiel_control_pushbutton_set_callback( struct Suiel_control_pushbutton *handle, enum EUIelCallPushButton select, int context, uiel_callback func )
{
    switch (select)
    {
        case UICpb_OK:
            handle->call_OK = func;
            handle->ccont_OK = (uint8)context;
            break;
        case UICpb_Esc:
            handle->call_Esc = func;
            handle->ccont_Esc = (uint8)context;
            break;
    }
}

void uiel_control_pushbutton_set_content( struct Suiel_control_pushbutton *handle, enum Eui_element_content cnt_type, int bitmap, char *text, enum Etextstyle style )
{
    handle->type = cnt_type;
    if ( cnt_type == uicnt_text )
    {
        int len;
        int i;
        int offsx;
        int offsy;

        handle->txt_style = style;
        len = strlen(text);
        if ( len > 15 )
        {
            strncpy( handle->content.text, text, 15 );
            len = 15;
        }
        else
            strcpy( handle->content.text, text );

        // calculate the text dimmensions
        grf_setup_font( (enum Etextstyle)handle->txt_style, 1, 0 );
        offsx = 0;
        offsy = Gtext_GetCharacterHeight();
        for (i=0; i<len; i++)
        {
            offsx += Gtext_GetCharacterWidth( text[i] ) + 1;
        }

        if ( offsx < handle->w)
            offsx = (handle->w - offsx)/2 + 1;
        else
            offsx = 0;

        if ( offsy < handle->h)
            offsy = (handle->h - offsy)/2 + 1;
        else
            offsy = 0;

        handle->offsx = offsx;
        handle->offsy = offsy;
    }
    else if ( cnt_type == uicnt_bitmap )
    {
        handle->content.bitmap = bitmap;
    }
}

static inline void uiel_control_pushbutton_display( struct Suiel_control_pushbutton *handle, bool focus )
{
    int xe, ye;

    xe = handle->xpoz+handle->w;
    ye = handle->ypoz+handle->h;

    if ( handle->type != uicnt_hollow )
    {
        uigrf_rounded_rect(handle->xpoz, handle->ypoz, xe, ye, 1, true, 0);

        if ( handle->type == uicnt_text )
        {
            Graphic_SetColor(1);
            grf_setup_font( (enum Etextstyle)handle->txt_style, 1, 0 );
            uigrf_text( handle->xpoz + handle->offsx, handle->ypoz + handle->offsy, (enum Etextstyle)handle->txt_style, handle->content.text );
        }
        else
        {
            uibm_put_bitmap( handle->xpoz + 1, handle->ypoz + 1, handle->content.bitmap );
        }
    }

    if ( focus )
    {
        if ( (handle->ID & ELEM_IN_FOCUS) == 0 )
        {
            int_focus = 0;  // the control just got the focus, reset the internal focus state
            handle->ID |= ELEM_IN_FOCUS;
        }

        if ( handle->type != uicnt_hollow )
            Graphic_Rectangle( handle->xpoz+1, handle->ypoz+1, xe-1, ye-1 );
        else
            uigrf_rounded_rect( handle->xpoz, handle->ypoz, xe, ye, 1, false, 0);
    }
    else
    {
        handle->ID &= ~ELEM_IN_FOCUS;
        if ( handle->type == uicnt_hollow )
            uigrf_rounded_rect( handle->xpoz, handle->ypoz, xe, ye, 0, false, 0);
    }
}



/// NUMERIC CONTROL

void uiel_control_numeric_init( struct Suiel_control_numeric *handle, int min, int max, int inc, int xpoz, int ypoz, int length, char padding, enum Etextstyle style )
{
    memset( handle, 0, sizeof(*handle));

    if ( handle->val > 0 )
        handle->val = min;
    else
        handle->val = 0;

    handle->ID = ELEM_ID_NUMERIC;
    handle->min = min;
    handle->max = max;
    handle->inc = inc;
    handle->xpoz = xpoz;
    handle->ypoz = ypoz;
    handle->lenght = length;
    handle->padding = padding;
    handle->style = (uint8)style;

    grf_setup_font( style, 1, 0 );

    handle->width = (Gtext_GetCharacterWidth('0') + 1) * length;
    handle->height = Gtext_GetCharacterHeight();
}

void uiel_control_numeric_set_callback( struct Suiel_control_numeric *handle, enum EUIelCallNum select, int context, uiel_callback func )
{
    switch (select)
    {
        case UICnum_OK:
            handle->call_OK = func;
            handle->ccont_OK = (uint8)context;
            break;
        case UICnum_Esc:
            handle->call_Esc = func;
            handle->ccont_Esc = (uint8)context;
            break;
        case UICnum_EscLong:
            handle->call_EscLong = func;
            handle->ccont_EscLong = (uint8)context;
            break;
        case UICnum_Vchange:
            handle->call_Vchange = func;
            handle->ccont_Vchange = (uint8)context;
            break;
    }
}

void uiel_control_numeric_set( struct Suiel_control_numeric *handle, int val )
{
    if ( val < handle->min )
        handle->val = handle->min;
    else if ( val > handle->max )
        handle->val = handle->max;
    else
        handle->val = val;
}

int uiel_control_numeric_get( struct Suiel_control_numeric *handle )
{
    return handle->val;
}

void uiel_control_numeric_inc( struct Suiel_control_numeric *handle )
{
    if ( (handle->val + handle->inc) > handle->max )
        handle->val = handle->max;
    else
        handle->val += handle->inc;

    if ( handle->call_Vchange )
        handle->call_Vchange( handle->ccont_Vchange, (void*)&handle->val );
}

void uiel_control_numeric_dec( struct Suiel_control_numeric *handle )
{
    if ( (handle->val - handle->inc) < handle->min )
        handle->val = handle->min;
    else
        handle->val -= handle->inc;

    if ( handle->call_Vchange )
        handle->call_Vchange( handle->ccont_Vchange, (void*)&handle->val );
}

static inline void uiel_control_numeric_display( struct Suiel_control_numeric *handle, bool focus )
{
    int xend = handle->xpoz + handle->width + 1;
    int yend = handle->ypoz + handle->height + 1;
    Graphic_SetColor(0);
    Graphic_FillRectangle( handle->xpoz+1, handle->ypoz+1, xend, yend, 0 );
    uigrf_putnr( handle->xpoz+2, handle->ypoz+2, (enum Etextstyle)handle->style, handle->val, handle->lenght, handle->padding, false );

    xend++;
    yend++;

    if ( focus)
    {
        if ( (handle->ID & ELEM_IN_FOCUS) == 0 )
        {
            int_focus = 0;  // the control just got the focus, reset the internal focus state
            handle->ID |= ELEM_IN_FOCUS;
        }

        Graphic_SetColor(1);
        Graphic_Rectangle( handle->xpoz, handle->ypoz, xend, yend );
        Graphic_PutPixel( handle->xpoz, handle->ypoz, -1 );
        Graphic_PutPixel( xend, handle->ypoz, -1 );
        Graphic_PutPixel( handle->xpoz, yend, -1 );
        Graphic_PutPixel( xend, yend, -1 );

        if ( int_focus )
        {
            Graphic_SetColor( -1 );
            Graphic_FillRectangle( handle->xpoz, handle->ypoz, xend, yend, -1 );
        }
    }
    else
    {
        Graphic_SetColor(0);
        Graphic_Rectangle( handle->xpoz, handle->ypoz, xend, yend );
        handle->ID &= ~ELEM_IN_FOCUS;
    }

}


/// EDIT BOX

void uiel_control_edit_init( struct Suiel_control_edit *handle, int xpoz, int ypoz, enum Etextstyle style, enum Eui_edit_type type, int nr_chars, int decpoint )
{
    uint32 w_d;
    uint32 w_s;

    memset( handle, 0, sizeof(*handle));
    grf_setup_font( style, 1, 0 );

    handle->ID = ELEM_ID_EDIT;
    handle->type = type;
    handle->textstyle = (uint8)style;
    handle->xpoz = xpoz;
    handle->ypoz = ypoz;
    handle->chars = nr_chars;
    handle->decpoint = decpoint;

    handle->yend = ypoz + Gtext_GetCharacterHeight() + 2;

    w_s = Gtext_GetCharacterWidth('.') + 1;
    grf_setup_font( (enum Etextstyle)(style | uitxt_MONO), 1, -1 );
    w_d = Gtext_GetCharacterWidth('0') + 1;

    handle->xend = xpoz + nr_chars * w_d + 2;
    if ( decpoint )
        handle->xend += w_s;

    handle->dwidth = (uint8)w_d;
    handle->swidth = (uint8)w_s;

    if ( type == uiedit_numeric )
    {
        handle->length = nr_chars + ( (decpoint!=0) ? 1 : 0 );
        uiel_control_edit_set_num( handle, 0 );
    }
    else
    {
        memset( handle->content, ' ', nr_chars );
        handle->length = nr_chars;
    }
}

void uiel_control_edit_set_callback( struct Suiel_control_edit *handle, enum EUIelCallEdit select, int context, uiel_callback func )
{
    switch (select)
    {
        case UICedit_EditDone:
            handle->call_EditDone = func;
            handle->ccont_EditDone = (uint8)context;
            break;
        case UICedit_Esc:
            handle->call_Esc = func;
            handle->ccont_Esc = (uint8)context;
            break;
        case UICedit_EscLong:
            handle->call_EscLong = func;
            handle->ccont_EscLong = (uint8)context;
            break;
        case UICedit_Vchange:
            handle->call_Vchange = func;
            handle->ccont_Vchange = (uint8)context;
            break;
    }
}

void uiel_control_edit_set_text( struct Suiel_control_edit *handle, const char *string )
{
    int len;
    if ( handle->type == uiedit_numeric )
        return;

    len = strlen(string);
    if ( len > handle->length )
        len = handle->length;

    strncpy( handle->content, string, len );
}


void uiel_control_edit_set_num( struct Suiel_control_edit *handle, int val )
{
    char *str;
    char str1[14];
    int i;
    int len;
    int div = 1;

    str = handle->content;

    if ( handle->type != uiedit_numeric )
        return;

    // fill up the integer part
    len = handle->chars - handle->decpoint; // length of integer part or whole part

    for ( i=0; i<len; i++ )                 // '0' padding
        str[i]  = '0';

    div = poz_2_increment( handle->decpoint );    // convert the value
    itoa( val/div, str1, 10 );
    i = strlen( str1 );

    strncpy( (str + len - i), str1, i );    // insert the converted integer part before the decimal point or before the end of the string

    str += len;
    // add the decimal point
    if ( handle->decpoint )
    {
        str[0] = '.';
        str++;

        // add the rest
        len = handle->decpoint;
        for ( i=0; i<len; i++ )                 // '0' padding
            str[i]  = '0';

        val = val % div;
        itoa( val, str1, 10 );
        i = strlen( str1 );

        strncpy( str, str1, i );    // insert the converted nr.
        str += i;
    }
    str[handle->length] = 0x00;
}


const char *uiel_control_edit_get_text( struct Suiel_control_edit *handle )
{
    return handle->content;
}

int uiel_control_edit_get_num( struct Suiel_control_edit *handle )
{
    int val = 0;
    int len;
    char *str = handle->content;
    char str1[14];

    len = handle->chars - handle->decpoint;
    strncpy( str1, str, len );
    if ( handle->decpoint )
    {
        str += len + 1;
        strncpy( str1 + len, str, handle->decpoint );
    }

    str1[ handle->chars ] = 0;
    val = atoi( str1 );
    return val;
}


static void uiel_control_edit_incdec_internal_send_valch_callback( struct Suiel_control_edit *handle )
{
    if ( handle->call_Vchange )
    {
        if ( handle->type == uiedit_numeric )
        {
            int val;
            val = uiel_control_edit_get_num(handle);
            handle->call_Vchange( handle->ccont_Vchange, (void*)&val );
        }
        else
            handle->call_Vchange( handle->ccont_Vchange, (void*)handle->content );
    }
}

static void uiel_control_edit_inc( struct Suiel_control_edit *handle )
{
    if ( (int_focus & 0x80) == 0 )
    {
        if ( int_focus < handle->chars )
            int_focus++;
    }
    else
    {
        char *chr;
        int index = handle->chars - (int_focus & 0x1f);
        if ( handle->decpoint &&
             ( index >= (handle->chars - handle->decpoint)) )
            index++;

        chr = &handle->content[index];

        switch ( handle->type )
        {
            case uiedit_numeric:
                if ( *chr == '9' )
                    *chr = '0';
                else
                    (*chr)++;
                break;
            case uiedit_alpha_upcase:
                if ( *chr == '`' )
                    *chr = ' ';
                else
                    (*chr)++;
                break;
            case uiedit_alpha_all:
                if ( *chr == 'z' )
                    *chr = ' ';
                else
                    (*chr)++;
                break;
        }

        uiel_control_edit_incdec_internal_send_valch_callback( handle );
    }
}


static void uiel_control_edit_dec( struct Suiel_control_edit *handle )
{
    if ( (int_focus & 0x80) == 0 )
    {
        if ( int_focus > 1 )
            int_focus--;
    }
    else
    {
        char *chr;
        int index = handle->chars - (int_focus & 0x1f);
        if ( handle->decpoint &&
             ( index >= (handle->chars - handle->decpoint)) )
            index++;

        chr = &handle->content[index];

        switch ( handle->type )
        {
            case uiedit_numeric:
                if ( *chr == '0' )
                    *chr = '9';
                else
                    (*chr)--;
                break;
            case uiedit_alpha_upcase:
                if ( *chr == ' ' )
                    *chr = '`';
                else
                    (*chr)--;
                break;
            case uiedit_alpha_all:
                if ( *chr == ' ' )
                    *chr = 'z';
                else
                    (*chr)--;
                break;
        }

        uiel_control_edit_incdec_internal_send_valch_callback(handle);
    }
}

static void uiel_control_edit_reset_digit( struct Suiel_control_edit *handle )
{
    if ( (int_focus & 0x80) == 0 )
        return;

    int index = handle->chars - (int_focus & 0x1f);

    if ( handle->type == uiedit_numeric )
        handle->content[index] = '0';
    else
        handle->content[index] = ' ';
}



static inline void uiel_control_edit_display( struct Suiel_control_edit *handle, bool focus )
{
    Graphic_SetColor(0);
    Graphic_FillRectangle( handle->xpoz+1, handle->ypoz+1, handle->xend-1, handle->yend-1, 0 );
    uigrf_text_mono( handle->xpoz+2, handle->ypoz+2, (enum Etextstyle)handle->textstyle, handle->content, (handle->decpoint) ? true : false );

    if ( focus )
    {
        if ( (handle->ID & ELEM_IN_FOCUS) == 0 )
        {
            int_focus = 0;  // the control just got the focus, reset the internal focus state
            handle->ID |= ELEM_IN_FOCUS;
        }

        Graphic_SetColor(1);
        Graphic_Rectangle( handle->xpoz, handle->ypoz, handle->xend, handle->yend );
        Graphic_PutPixel( handle->xpoz, handle->ypoz, -1 );
        Graphic_PutPixel( handle->xend, handle->ypoz, -1 );
        Graphic_PutPixel( handle->xpoz, handle->yend, -1 );
        Graphic_PutPixel( handle->xend, handle->yend, -1 );

        if ( int_focus )
        {
            int xf;
            int xff;
            int index = (handle->chars - (int_focus & 0x1f) );

            xf = index * handle->dwidth ;
            xff = xf + handle->dwidth + 2;

            if ( int_focus & 0x80 )     // edit element
            {
                Graphic_SetWidth( 0 );
                Graphic_FillRectangle( handle->xpoz + xf + 1, handle->ypoz + 1, handle->xpoz + xff - 1, handle->yend - 1, -1 );
                Graphic_SetWidth( 1 );
            }
            else                            // select element
            {
                Graphic_SetColor( 1 );
                Graphic_Rectangle( handle->xpoz + xf, handle->ypoz + 1, handle->xpoz + xff, handle->yend - 1 );
            }
        }
    }
    else
    {
        Graphic_SetColor(0);
        Graphic_Rectangle( handle->xpoz, handle->ypoz, handle->xend, handle->yend );
        handle->ID &= ~ELEM_IN_FOCUS;
    }
}






/// EDIT BOX

static void internal_time_incdec( struct Suiel_control_time *handle, bool increment )
{
    if ( int_focus & 0x80 )     // selected for editing
    {
        switch ( int_focus & 0x1f)
        {
        case 1:     // seconds
            if (increment )
            {
                handle->time.second++;
                if ( handle->time.second == 60 )
                    handle->time.second = 0;
            }
            else
            {
                if ( handle->time.second == 0 )
                    handle->time.second = 59;
                else
                    handle->time.second--;
            }
            break;
        case 2:
            if (increment)
            {
                handle->time.minute++;
                if ( ((handle->time.minute == 60) && (handle->minute_only == false)) ||
                     ((handle->time.minute == 100) && (handle->minute_only))            )
                    handle->time.minute = 0;
            }
            else
            {
                if ( handle->time.minute == 0 )
                {
                    if ( handle->minute_only )
                        handle->time.minute = 99;
                    else
                        handle->time.minute = 59;
                }
                else
                    handle->time.minute--;
            }
            break;
        case 3:
            if (increment )
            {
                handle->time.hour++;
                if ( handle->time.hour == 20 )
                    handle->time.hour = 0;
            }
            else
            {
                if ( handle->time.hour == 0 )
                    handle->time.hour = 19;
                else
                    handle->time.hour--;
            }
            break;
        }

        if ( handle->call_Vchange )
            handle->call_Vchange( handle->ccont_Vchange, (void*)&handle->time );

    }
    else                    // select digit
    {
        if ( increment == true )
        {
            if ( ((int_focus < 3) && (handle->minute_only == false)) ||
                 ((int_focus < 2) && (handle->minute_only == true))    )
                int_focus++;
        } else if ( int_focus > 1 )
            int_focus--;
    }
}

static void internal_time_reset_digit( struct Suiel_control_time *handle )
{
    if ( (int_focus & 0x80) == 0 )
        return;

    if ( (int_focus & 0x1f) == 1 )
        handle->time.second = 0;
    else if ( (int_focus & 0x1f) == 2 )
        handle->time.minute = 0;
    else
        handle->time.hour = 0;
}


void uiel_control_time_init( struct Suiel_control_time *handle, int xpoz, int ypoz, bool minute_only, enum Etextstyle style )
{
    uint32 w_d;
    uint32 w_s;

    memset( handle, 0, sizeof(*handle) );

    grf_setup_font( style, 1, 0 );

    handle->ID = ELEM_ID_TIME;
    handle->minute_only = (uint8)minute_only;
    handle->textstyle = (uint8)style;
    handle->xpoz = (uint16)xpoz;
    handle->ypoz = (uint16)ypoz;
    handle->yend = ypoz + Gtext_GetCharacterHeight() + 2;

    w_d = Gtext_GetCharacterWidth('0') + 1;
    w_s = Gtext_GetCharacterWidth(':') + 1;

    handle->dwidth = (uint8)w_d;
    handle->swidth = (uint8)w_s;

    if ( minute_only )
        handle->xend = xpoz + 4 * w_d + w_s + 2;
    else
        handle->xend = xpoz + 6 * w_d + 2 * w_s + 2;
}

void uiel_control_time_set_callback( struct Suiel_control_time *handle, enum EUIelCallTime select, int context, uiel_callback func )
{
    switch (select)
    {
        case UICtime_EditDone:
            handle->call_EditDone = func;
            handle->ccont_EditDone = (uint8)context;
            break;
        case UICtime_Esc:
            handle->call_Esc = func;
            handle->ccont_Esc = (uint8)context;
            break;
        case UICtime_EscLong:
            handle->call_EscLong = func;
            handle->ccont_EscLong = (uint8)context;
            break;
        case UICtime_Vchange:
            handle->call_Vchange = func;
            handle->ccont_Vchange = (uint8)context;
            break;
    }
}

void uiel_control_time_set_time( struct Suiel_control_time *handle, timestruct time )
{
    handle->time = time;
}


void uiel_control_time_get_time( struct Suiel_control_time *handle, timestruct *time )
{
    *time = handle->time;
}

static inline void uiel_control_time_display( struct Suiel_control_time *handle, bool focus )
{
    uigrf_puttime( handle->xpoz+2, handle->ypoz+2, (enum Etextstyle)handle->textstyle, 1, handle->time, handle->minute_only, true );
    if ( focus )
    {
        if ( (handle->ID & ELEM_IN_FOCUS) == 0 )
        {
            int_focus = 0;  // the control just got the focus, reset the internal focus state
            handle->ID |= ELEM_IN_FOCUS;
        }

        Graphic_SetColor(1);
        Graphic_Rectangle( handle->xpoz, handle->ypoz, handle->xend, handle->yend );
        Graphic_PutPixel( handle->xpoz, handle->ypoz, -1 );
        Graphic_PutPixel( handle->xend, handle->ypoz, -1 );
        Graphic_PutPixel( handle->xpoz, handle->yend, -1 );
        Graphic_PutPixel( handle->xend, handle->yend, -1 );

        if ( int_focus )
        {
            int xf;
            int xff;
            int offset = 0;

            if ( handle->minute_only == false )
                offset += handle->dwidth * 2 + handle->swidth;

            switch ( int_focus & 0x1f )
            {
                case 1:     // seconds
                    xf = offset + handle->dwidth * 2 + handle->swidth;
                    break;
                case 2:     // minutes
                    xf = offset;
                    break;
                case 3:     // hours
                    xf = 0;
                    break;
            }
            xff = xf + handle->dwidth * 2 + 2;
            if ( int_focus & 0x80 )     // edit element
            {
                Graphic_SetColor( -1 );
                Graphic_FillRectangle( handle->xpoz + xf, handle->ypoz + 1, handle->xpoz + xff, handle->yend - 1, -1 );
            }
            else                            // select element
            {
                Graphic_SetColor( 1 );
                Graphic_Rectangle( handle->xpoz + xf, handle->ypoz + 1, handle->xpoz + xff, handle->yend - 1 );
            }
        }
    }
    else
    {
        Graphic_SetColor(0);
        Graphic_Rectangle( handle->xpoz, handle->ypoz, handle->xend, handle->yend );
        handle->ID &= ~ELEM_IN_FOCUS;
    }
}



// UI element display routine
void ui_element_display( void *handle, bool focus )
{
    uint32 handle_ID;
    handle_ID = ( (*( (uint32*)handle )) & ~ELEM_IN_FOCUS );
    switch ( handle_ID )
    {
        case ELEM_ID_CHECKBOX:      uiel_control_checkbox_display( (struct Suiel_control_checkbox *)handle, focus ); break;
        case ELEM_ID_PUSHBUTTON:    uiel_control_pushbutton_display( (struct Suiel_control_pushbutton *)handle, focus); break;
        case ELEM_ID_NUMERIC:       uiel_control_numeric_display( (struct Suiel_control_numeric *)handle, focus ); break;
        case ELEM_ID_EDIT:          uiel_control_edit_display( (struct Suiel_control_edit *)handle, focus ); break;
        case ELEM_ID_TIME:          uiel_control_time_display( (struct Suiel_control_time *)handle, focus ); break;
        case ELEM_ID_LIST:          uiel_control_list_display( (struct Suiel_control_list *)handle, focus ); break;
        case ELEM_ID_DROPDOWN_MENU: uiel_dropdown_menu_display( (struct Suiel_dropdown_menu *)handle ); break;
    }
}

// UI Polling routine
bool ui_element_poll( void *handle, struct SEventStruct *evmask )
{
    bool changed = false;           // display change
    uint32 handle_ID;

    handle_ID = ( (*( (uint32*)handle )) & ~ELEM_IN_FOCUS );

    if ( evmask->key_event )
    {
        if ( evmask->key_pressed & KEY_OK )
        {
            switch ( handle_ID )
            {
                case ELEM_ID_NUMERIC:       changed = internal_control_numeric_OK( (struct Suiel_control_numeric *)handle ); break;
                case ELEM_ID_EDIT:          changed = internal_control_edit_OK( (struct Suiel_control_edit *)handle ); break;
                case ELEM_ID_TIME:          changed = internal_control_time_OK( (struct Suiel_control_time *)handle ); break;
                case ELEM_ID_LIST:          changed = internal_control_list_OK( (struct Suiel_control_list *)handle ); break;
                case ELEM_ID_CHECKBOX:      changed = internal_control_checkbox_OK( (struct Suiel_control_checkbox *)handle ); break; 
                case ELEM_ID_PUSHBUTTON:    changed = internal_control_pbutton_OK( (struct Suiel_control_pushbutton *)handle ); break; 
                case ELEM_ID_DROPDOWN_MENU: changed = internal_control_menu_OK( (struct Suiel_dropdown_menu *)handle ); break;
            }

            evmask->key_pressed &= ~KEY_OK;     // delete the keypress event for the upper layer as it is captured by this layer
        }

        // Up and Down are captured for any element in focus and have immediate action
        {
            if ( evmask->key_pressed & KEY_UP )
            {
                switch ( handle_ID )
                {
                    case ELEM_ID_NUMERIC:       uiel_control_numeric_inc( (struct Suiel_control_numeric *)handle ); break;
                    case ELEM_ID_EDIT:          uiel_control_edit_inc( (struct Suiel_control_edit *)handle ); break;
                    case ELEM_ID_TIME:          internal_time_incdec( (struct Suiel_control_time *)handle, true ); break;
                    case ELEM_ID_LIST:          uiel_control_list_set_next( (struct Suiel_control_list *)handle); break;
                    case ELEM_ID_DROPDOWN_MENU: uiel_dropdown_menu_set_prew( (struct Suiel_dropdown_menu *)handle ); break;
                }
                changed = true;
                evmask->key_pressed &= ~KEY_UP;     // delete the keypress event for the upper layer as it is captured by this layer
            }
            else if ( evmask->key_pressed & KEY_DOWN )
            {
                switch ( handle_ID )
                {
                    case ELEM_ID_NUMERIC:       uiel_control_numeric_dec( (struct Suiel_control_numeric *)handle ); break;
                    case ELEM_ID_EDIT:          uiel_control_edit_dec( (struct Suiel_control_edit *)handle ); break;
                    case ELEM_ID_TIME:          internal_time_incdec( (struct Suiel_control_time *)handle, false ); break;
                    case ELEM_ID_LIST:          uiel_control_list_set_prew( (struct Suiel_control_list *)handle); break;
                    case ELEM_ID_DROPDOWN_MENU: uiel_dropdown_menu_set_next( (struct Suiel_dropdown_menu *)handle ); break;
                }
                changed = true;
                evmask->key_pressed &= ~KEY_DOWN;     // delete the keypress event for the upper layer as it is captured by this layer
            }
        }

        if ( evmask->key_longpressed & KEY_ESC )
        {
            switch ( handle_ID )
            {
                case ELEM_ID_LIST:          internal_control_list_longEsc((struct Suiel_control_list *)handle); break;
                case ELEM_ID_EDIT:          internal_control_edit_longEsc((struct Suiel_control_edit *)handle); break;
                case ELEM_ID_TIME:          internal_control_time_longEsc((struct Suiel_control_time *)handle); break;
                case ELEM_ID_NUMERIC:       internal_control_numeric_longEsc((struct Suiel_control_numeric *)handle); break;
            }
            changed = true;
            evmask->key_longpressed &= ~KEY_ESC;
        }

        if ( evmask->key_released & KEY_ESC )
        {
            switch ( handle_ID )
            {
                case ELEM_ID_NUMERIC:       changed = internal_control_numeric_ESC( (struct Suiel_control_numeric *)handle ); break;
                case ELEM_ID_EDIT:          changed = internal_control_edit_ESC( (struct Suiel_control_edit *)handle ); break;
                case ELEM_ID_TIME:          changed = internal_control_time_ESC( (struct Suiel_control_time *)handle ); break;
                case ELEM_ID_LIST:          changed = internal_control_list_ESC( (struct Suiel_control_list *)handle ); break;
                case ELEM_ID_CHECKBOX:      changed = internal_control_checkbox_ESC( (struct Suiel_control_checkbox *)handle ); break; 
                case ELEM_ID_PUSHBUTTON:   changed = internal_control_pbutton_ESC( (struct Suiel_control_pushbutton *)handle ); break;
                case ELEM_ID_DROPDOWN_MENU: changed = internal_control_menu_ESC( (struct Suiel_dropdown_menu *)handle ); break;
            }

            evmask->key_released &= ~KEY_ESC;
        }

        /*    else if ( evmask->key_pressed & KEY_ESC )
            {
                if ( int_focus & 0x80)
                {
                    switch ( handle_ID )
                    {
                        case ELEM_ID_EDIT:          uiel_control_edit_reset_digit( (struct Suiel_control_edit *)handle ); break;
                        case ELEM_ID_TIME:          internal_time_reset_digit( (struct Suiel_control_time *)handle ); break;
                    }
                    int_focus &= ~0x80;
                }
                else
                {
                    int_focus = 0;
                }

                if ( handle_ID != ELEM_ID_DROPDOWN_MENU )
                {
                    evmask->key_pressed &= ~KEY_ESC;
                    changed = true;
                }
            }
            */


    }

    if ( changed )
        ui_element_display( handle, true );
    return changed;
}


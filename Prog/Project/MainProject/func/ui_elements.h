#ifndef UI_ELEMENTS_H
#define UI_ELEMENTS_H

#ifdef __cplusplus
    extern "C" {
#endif

    #include "stm32f10x.h"
    #include "typedefs.h"
    #include "ui_graphics.h"
    #include "events_ui.h"


    #define UIEL_MENU_TOTAL_MAX            128  // maximum menu bar length
    #define UIEL_LIST_LABEL_MAX            40   // maximum label list length
    #define UIEL_LIST_MAX                  10   // maximum list elements
    #define UIEL_TEXT_MAX                  32   // maximum text length in an editor box

    #define ELEM_ID_DROPDOWN_MENU   94
    #define ELEM_ID_LIST            95
    #define ELEM_ID_CHECKBOX        96
    #define ELEM_ID_NUMERIC         97
    #define ELEM_ID_EDIT            98
    #define ELEM_ID_TIME            99

    #define ELEM_IN_FOCUS           0x8000

    enum Eui_edit_type
    {
        uiedit_numeric  = 0,
        uiedit_alpha_upcase,
        uiedit_alpha_all
    };


    struct Suiel_dropdown_menu
    {
        uint32 ID;              // element ID
        uint16 poz_x1;          //
        uint16 poz_y1;          //  menu window
        uint16 poz_x2;          //
        uint16 poz_y2;          //
        uint8 elem_total;       // total elements in list
        uint8 elem_crt;         // selected element - ( 0 -> elem_total-1 ) all elements
        uint8 ytext;            // text element height in pixels
        uint8 melem;            // elements scrollable in a menu window
        uint8 moffs;            // position offset inside of menu window (0 - melem-1)

        char labels[ UIEL_MENU_TOTAL_MAX ];
    };

    struct Suiel_control_list
    {
        uint32 ID;              // element ID
        uint16 poz_x;           //
        uint16 poz_y;           // menu window
        uint8 textstyle;        // font used for display
        uint8 elem_total;       // total elements in list
        uint8 elem_crt;         // selected element - ( 0 -> elem_total-1 ) all elements
        uint8 width;            // text element height in pixels
        uint8 height;           // elements displayed in a menu window

        char labels[ UIEL_LIST_LABEL_MAX ];
        uint16 value[ UIEL_LIST_MAX ];
    };

    struct Suiel_control_checkbox
    {
        uint32 ID;              // element ID
        uint16 xpoz;            // start coordinate (includes braces)
        uint16 ypoz;            // start coordinate
        bool set;
    };

    struct Suiel_control_numeric
    {
        uint32 ID;          // element ID
        int val;            // current value
        int min;            // minimum value
        int max;            // maximum value
        int inc;            // increment
        uint16 xpoz;        // start coordinate (includes braces)
        uint16 ypoz;        // start coordinate
        uint8 lenght;       // max digits displayed
        uint8 width;        // total width in pixels (used for masking)
        uint8 height;       // height in pixels
        uint8 style;        // character style
        char padding;       // padding character
    };


    struct Suiel_control_edit
    {
        uint32 ID;                  // element ID
        uint16 xpoz;                // start coordinate
        uint16 ypoz;                // start coordinate
        uint16 xend;                // end coordinates
        uint16 yend;                // end coordinates
        enum Eui_edit_type type;    // type
        uint8 dwidth;               // digit width
        uint8 swidth;               // separator width
        uint8 textstyle;            // font used for display
        uint8 chars;                // nr. of digits or characters
        uint8 decpoint;             // decimal point for numeric
        uint8 length;               // length of text (no braces)
        uint8 poz;                  // pozition in the text buffer
        char content[UIEL_TEXT_MAX];
    };

    struct Suiel_control_time
    {
        uint32 ID;                  // element ID
        timestruct time;            // time
        uint16 xpoz;                // start coordinate
        uint16 ypoz;                // - uses the upper-left corner, text will be displayed at +2 pixels on x and y
        uint16 xend;                // time box width
        uint16 yend;                // time box height
        uint8 dwidth;               // digit width
        uint8 swidth;               // spacer width
        uint8 minute_only;          // if 1 then 99:59 format (mm:ss)  and if 0 then 99:59:59 format
        uint8 textstyle;            // font used for display

    };


    // DROPDOWN MENU

    // inits or resets a dropdown menu
    void uiel_dropdown_menu_init( struct Suiel_dropdown_menu *handle, int xmin, int xmax, int ymin, int ymax );

    // add element to a dropdown menu
    // returns the menu element index
    int uiel_dropdown_menu_add_item( struct Suiel_dropdown_menu *handle, char *element );

    // move to right in sliding menu
    void uiel_dropdown_menu_set_next( struct Suiel_dropdown_menu *handle );

    // move to left in sliding menu
    void uiel_dropdown_menu_set_prew( struct Suiel_dropdown_menu *handle );

    // set the menu index
    void uiel_dropdown_menu_set_index( struct Suiel_dropdown_menu *handle, unsigned int index );

    // get the current index
    int uiel_dropdown_menu_get_index( struct Suiel_dropdown_menu *handle );



    // LIST

    // inits or resets a list
    void uiel_control_list_init( struct Suiel_control_list *handle, int xpoz, int ypoz, int width, enum Etextstyle style );

    // add element to a list
    // value can be 0 - 255
    // returns 0 on success
    int uiel_control_list_add_item( struct Suiel_control_list *handle, char *element, int value );

    // move up in list
    void uiel_control_list_set_next( struct Suiel_control_list *handle );

    // move down in list
    void uiel_control_list_set_prew( struct Suiel_control_list *handle );

    // set the list index
    void uiel_control_list_set_index( struct Suiel_control_list *handle, unsigned int index );

    // get the list index
    int uiel_control_list_get_index( struct Suiel_control_list *handle );

    // get the list value
    int uiel_control_list_get_value( struct Suiel_control_list *handle );


    /// CHECKBOX CONTROL

    void uiel_control_checkbox_init( struct Suiel_control_checkbox *handle, int xpoz, int ypoz );

    void uiel_control_checkbox_set( struct Suiel_control_checkbox *handle, bool set );

    bool uiel_control_checkbox_get( struct Suiel_control_checkbox *handle );


    /// NUMERIC CONTROL

    void uiel_control_numeric_init( struct Suiel_control_numeric *handle, int min, int max, int inc, int xpoz, int ypoz, int length, char padding, enum Etextstyle style );

    void uiel_control_numeric_set( struct Suiel_control_numeric *handle, int val );

    int  uiel_control_numeric_get( struct Suiel_control_numeric *handle );

    void uiel_control_numeric_inc( struct Suiel_control_numeric *handle );

    void uiel_control_numeric_dec( struct Suiel_control_numeric *handle );


    /// EDIT BOX

    void uiel_control_edit_init( struct Suiel_control_edit *handle, int xpoz, int ypoz, enum Etextstyle style, enum Eui_edit_type type, int nr_chars, int decpoint );

    void uiel_control_edit_set_text( struct Suiel_control_edit *handle, const char *string );

    void uiel_control_edit_set_num( struct Suiel_control_edit *handle, int val );

    const char *uiel_control_edit_get_text( struct Suiel_control_edit *handle );

    int  uiel_control_edit_get_num( struct Suiel_control_edit *handle );


    /// TIME DISPLAY

    void uiel_control_time_init( struct Suiel_control_time *handle, int xpoz, int ypoz, bool minute_only, enum Etextstyle style );

    void uiel_control_time_set_time( struct Suiel_control_time *handle, timestruct time );

    void uiel_control_time_get_time( struct Suiel_control_time *handle, timestruct *time );



    /// GENERIC UI ROUTINES

    void ui_element_display( void *handle, bool focus );

    bool ui_element_poll( void *handle, struct SEventStruct *evmask );



#ifdef __cplusplus
    }
#endif

#endif // UI_ELEMENTS_H

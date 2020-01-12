#include <libxfce4panel/xfce-panel-plugin.h>
#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include <time.h>
#include <math.h>


static gboolean
sample_size_changed (XfcePanelPlugin *plugin,
                     gint size, void *data) {
    GtkOrientation orientation;

    orientation = xfce_panel_plugin_get_orientation (plugin);

    if (orientation == GTK_ORIENTATION_HORIZONTAL)
        gtk_widget_set_size_request (GTK_WIDGET (plugin), -1, size);
    else
        gtk_widget_set_size_request (GTK_WIDGET (plugin), size, -1);

    return TRUE;
}
int WINDOW_W = 300, WINDOW_H = 250;

int SHOWN;
GtkWidget *p_window;
XfcePanelPlugin *plugins;
static cairo_surface_t *surface = NULL;
GtkWidget *area = NULL;

/* GAME FIELD STRUCTURE:
 * [7]  [X]       [A]  [B]  [C]  [D]
 * 0-51
 * [0]  [1]  [2]  [3]  [4]  [5]  [6]
 * 0-51 0-51 0-51 0-51 0-51 0-51 0-51 
 *
 * > int FIELD[8][52]:
 * FIELD[0-6] are the 7 decks
 * and FIELD[7] is the stock
 * each array has 52 positions and 
 * each position is an int (0-52) that 
 * the card that is on this spot,
 * 52 means no card
 * > int STOCK_IDS[5]:
 * STOCK_IDS[0] tells which card from the stock
 * is now visible on the position [X] in the scheme above
 * STOCK_IDS[1-4] shows progress for each card suit 
 * (positions A-D in the scheme)
 * 52 means no progress, 0 means only Ace,
 * 1 means Ace and "2", etc.
 */

int FIELD[8][52];
int STOCK_IDS[8];

// Back and fore ground colors RGBA (from 0.0 to 1.0 each)
gdouble BG_COLOR[4] = {0.18, 0.53, 0.03, 1.0};
gdouble FG_COLOR[4] = {0.16, 0.45, 0.03, 1.0};
gdouble CARD_COLOR[4] = {0.92, 0.97, 0.73, 1.0};
gdouble CARD_BACK[4] = {0.05, 0.35, 0.57, 1.0};
gdouble CARD_STROKE[4] = {0.0, 0.0, 0.0, 1.0};

int FONT_SIZE = 14;
char FONT_NAME[50] = {"Source Code Variable"};
char SUITS[4][4] = {{"♥"}, {"♠"}, {"♦"}, {"♣"}};

// ♥ ♠ ♦ ♣ 

char *CARDS[52][3] = {
    {"♥A"}, {"♥2"}, {"♥3"}, {"♥4"}, {"♥5"}, {"♥6"}, {"♥7"}, 
    {"♥8"}, {"♥9"}, {"♥10"}, {"♥J"}, {"♥Q"}, {"♥K"},

    {"♠A"}, {"♠2"}, {"♠3"}, {"♠4"}, {"♠5"}, {"♠6"}, {"♠7"},
    {"♠8"}, {"♠9"}, {"♠10"}, {"♠J"}, {"♠Q"}, {"♠K"},

    {"♦A"}, {"♦2"}, {"♦3"}, {"♦4"}, {"♦5"}, {"♦6"}, {"♦7"},
    {"♦8"}, {"♦9"}, {"♦10"}, {"♦J"}, {"♦Q"}, {"♦K"},

    {"♣A"}, {"♣2"}, {"♣3"}, {"♣4"}, {"♣5"}, {"♣6"}, {"♣7"},
    {"♣8"}, {"♣9"}, {"♣10"}, {"♣J"}, {"♣Q"}, {"♣K"}
};
int MOVING[14]; // If player is moving some cards, they will be here
// the last int (MOVING[14]) is the place we took card from
// 0-6 is the 7 piles, 7 means we took it from stock and
// 8-11 means we took this card from the ABCD block

void new_game() {
    STOCK_IDS[0] = 50;
    for (int i = 1; i < 5; i++)
	STOCK_IDS[i] = 52;
    int games[52];
    for (int i = 0; i < 52; i++) 
	games[i] = i;
    // ----- Randomizing the cards order -----
    int r, t, buff;
    for (int i = 0; i < 52; i++) {
	r = rand() % 52;
	t = rand() % 52;
	buff = games[r];
	games[r] = games[t];
	games[t] = buff;
    }
    // ----- Putting cards on the table -----
    int n = 0;
    for (int i = 0; i < 7; i++) {
	for (int j = 0; j <= i; j++) {
	    FIELD[i][j] = games[n];
	    n++;
	}
	for (int j = i + 1; j < 52; j++)
	    FIELD[i][j] = 52;
    }
    int m = 0;
    while (n < 52) {
        FIELD[7][m] = games[n];
	n++;
	m++;
    }
    while (m < 52) {
	FIELD[7][m] = 52;
	m++;
    }
	
	
}


void draw_rect(GtkWidget *widget, cairo_t *cr, gpointer data, gdouble colors[4],
		int x, int y, int width, int height) {
    GdkRGBA color;
    GtkStyleContext *context;

    context = gtk_widget_get_style_context (widget);
    gtk_render_background (context, cr, 0, 0, WINDOW_W, WINDOW_H);

    color.red = colors[0];
    color.green = colors[1];
    color.blue = colors[2];
    color.alpha = colors[3];

    gdk_cairo_set_source_rgba (cr, &color);
    cairo_rectangle(cr, x, y, width, height);
    cairo_fill (cr);
}

int CLICKED = 0, X = 0, Y = 0;
int ROTATED = 0;

gboolean render(GtkWidget *widget, cairo_t *cr, gpointer data) {
    cairo_select_font_face (cr, FONT_NAME, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    while (gtk_events_pending ())
        gtk_main_iteration ();	
    int card_w = floor(WINDOW_W / 7);
    int card_h = floor(WINDOW_H / 5);
    int d = floor(WINDOW_W / 50); // Distance between cards;
    int thickness = 2; // thickness of card stroke
    cairo_set_font_size (cr, FONT_SIZE);
    // checking if the game is won
    if ((STOCK_IDS[1] == 12) && (STOCK_IDS[2] == 12) &&
             (STOCK_IDS[3] == 12) && (STOCK_IDS[4] == 12)) {
 	draw_rect(widget, cr, data, CARD_COLOR, 0, 0, WINDOW_W, WINDOW_H);
        cairo_set_source_rgb (cr, 0, 0, 0);
        cairo_move_to (cr, 0, FONT_SIZE + 3);
        cairo_show_text (cr, "CONGRATULATIONS !!!!");
        cairo_move_to (cr, 0, 3 * FONT_SIZE + 3);
        cairo_show_text (cr, "New game? Click");
	gtk_widget_queue_draw_area(p_window, 0, 0, WINDOW_W, WINDOW_H);
	if (CLICKED)
	   new_game();
	return FALSE;
    }
		        

    // drawing background
    draw_rect(widget, cr, data, BG_COLOR, 0, 0, WINDOW_W, WINDOW_H);

    // drawing deck slots
    for (int i = 0; i < 7; i++) {
        draw_rect(widget, cr, data, FG_COLOR,
		    card_w * i + d, card_h + d, card_w - d, card_h - d);
    }
    // drawing ABCD, X and stock dark outlines
    for (int i = 0; i < 7; i++) {
        draw_rect(widget, cr, data, FG_COLOR,
		    card_w * i + d, d, card_w - d, card_h - d);
    }
    draw_rect(widget, cr, data, BG_COLOR,
                card_w * 2 + d, d, card_w - d, card_h - d);
    for (int i = 0; i < 4; i++) {
	if (STOCK_IDS[i + 1] == 52) {
	    if (i % 2 == 0)
                cairo_set_source_rgb (cr, 90, 0, 0);
	    else
                cairo_set_source_rgb (cr, 0, 0, 0);
            cairo_move_to (cr, card_w * (i + 3) + card_w / 2 + 
			    d - FONT_SIZE / 2, card_h / 2 + d + 3);
            cairo_show_text (cr, SUITS[i]);
	}
	else {
            draw_rect(widget, cr, data, CARD_STROKE, 
			    card_w * (i + 3) + d - thickness, d - thickness, 
			    card_w - d, card_h - d);
            draw_rect(widget, cr, data, CARD_COLOR,
		    card_w * (i + 3) + d, d, card_w - d, card_h - d);
	    if (i % 2 == 0)
                cairo_set_source_rgb (cr, 90, 0, 0);
	    else
                cairo_set_source_rgb (cr, 0, 0, 0);
            cairo_move_to (cr, card_w * (i + 3) + d, d + FONT_SIZE - 3);
            cairo_show_text (cr, *CARDS[STOCK_IDS[i + 1] + 13 * i]);
	}
    }

    // drawing the 7 piles
    int n = 0;
    for (int i = 0; i < 7; i++) {
	n = 0;
	while (FIELD[i][n] != 52){
            draw_rect(widget, cr, data, CARD_STROKE,
	              card_w * i + d - thickness, 
		      card_h + d + floor(n * card_h / 3) - thickness,
		      card_w - d + thickness, card_h - d + thickness);
            draw_rect(widget, cr, data, CARD_COLOR,
	              card_w * i + d, card_h + d + floor(n * card_h / 3),
		      card_w - d, card_h - d);
	    if (FIELD[i][n] % 26 < 13)
                cairo_set_source_rgb (cr, 90, 0, 0);
	    else
                cairo_set_source_rgb (cr, 0, 0, 0);
	
            cairo_move_to (cr, card_w * i + d, 
			    card_h + d + floor(n * card_h / 3) + FONT_SIZE - 3);
            cairo_show_text (cr, *CARDS[FIELD[i][n]]);
	    n++;
	}
    } 
    // ----- Drawing the stock and X -----
    if (((FIELD[7][STOCK_IDS[0] + 1] < 52) || (FIELD[7][STOCK_IDS[0]] == 52))
		    && (FIELD[7][0] < 52)) {
        draw_rect(widget, cr, data, CARD_STROKE,
                  d - thickness, d - thickness,
                  card_w - d + thickness, card_h - d + thickness);
        draw_rect(widget, cr, data, CARD_BACK,
                  d, d, card_w - d, card_h - d);
    }
    if (FIELD[7][STOCK_IDS[0]] != 52) {
        draw_rect(widget, cr, data, CARD_STROKE,
	          card_w + d - thickness, d - thickness,
	          card_w - d + thickness, card_h - d + thickness);
        draw_rect(widget, cr, data, CARD_COLOR,
	          card_w + d, d, card_w - d, card_h - d);
	if (FIELD[7][STOCK_IDS[0]] % 26 < 13)
            cairo_set_source_rgb (cr, 90, 0, 0);
	else
            cairo_set_source_rgb (cr, 0, 0, 0);
	
        cairo_move_to (cr, card_w + d, d + FONT_SIZE - 3);
        cairo_show_text (cr, *CARDS[FIELD[7][STOCK_IDS[0]]]);
    }
	

    // ----- Drawing moving cards -----
    if (CLICKED) {
        // Moving a line of cards
	if ((Y > card_h + d) && (MOVING[0] == 52)) {
	   int i = X / card_w;
           int j = (Y - card_h - d) / lroundf(card_h / 3);
	   if ((FIELD[i][j - 1] < 52) && (FIELD[i][j] == 52) && (j > 0)) 
	       j--;
	   if ((FIELD[i][j - 2] < 52) && (FIELD[i][j - 1] == 52) && (j > 1))
	       j -= 2;

	   if (FIELD[i][j] < 52) {
	       for (int a = j; a < 51; a++) {
		   if (FIELD[i][a] == 52)
		       break;
		   // If you should not be able to move this card 
		   // (e.g. it is greater or the same color)
		   if (((FIELD[i][a] % 13 != FIELD[i][a + 1] % 13 + 1) || 
	         	     (abs(FIELD[i][a] / 13 - 
				  FIELD[i][a + 1] / 13) % 2 != 1)) && 
			   FIELD[i][a + 1] != 52) {
		       for (int b = 0; b < 14; b++)
			    MOVING[b] = 52;
		       break;
		   }
		   MOVING[a - j] = FIELD[i][a];
		   MOVING[a - j + 1] = FIELD[i][a + 1];
		   MOVING[13] = i;
	       }
	       if ((j == 52) || (FIELD[i][j + 1] == 52)) {
		   MOVING[0] = FIELD[i][j];
		   MOVING[13] = i;
		   for (int b = 1; b < 13; b++)
		       MOVING[b] = 52;
	       }
	       if (MOVING[0] < 52) {
		   for (int _ = j; _ < 52; _++)
		       FIELD[i][_] = 52;
	       }
	   }
	}
	// Drawing the cards that are being moved
        n = 0;
	while (MOVING[n] < 52) {
            draw_rect(widget, cr, data, CARD_STROKE,
	              X - card_w / 2 - thickness, 
		      Y - d - thickness + n * (card_h / 3),
		      card_w - d + thickness, card_h - d + thickness);
            draw_rect(widget, cr, data, CARD_COLOR,
	              X - card_w / 2, 
		      Y - d + n * (card_h / 3),
		      card_w - d, card_h - d);
	    if (MOVING[n] % 26 < 13)
                cairo_set_source_rgb (cr, 90, 0, 0);
	    else
                cairo_set_source_rgb (cr, 0, 0, 0);
	
            cairo_move_to (cr, X - card_w / 2, 
			    Y - d + floor(n * card_h / 3) + FONT_SIZE - 3);
            cairo_show_text (cr, *CARDS[MOVING[n]]);
	    n++;
	}
	// Rotating stock, taking card from it and taking card from ABCD
	if ((Y < card_h + d) && (MOVING[0] == 52)) {
           if ((X < card_w) && (!ROTATED)) {
	       ROTATED = 1;
	       if (FIELD[7][STOCK_IDS[0]] == 52)
                   STOCK_IDS[0] = 0; 
	       else
	           STOCK_IDS[0]++;
	   }
	   if ((X > card_w + d) && (X < 2 * card_w + d) &&
                    FIELD[7][STOCK_IDS[0]] < 52) {
	       MOVING[0] = FIELD[7][STOCK_IDS[0]];
	       MOVING[13] = 7;
	       n = STOCK_IDS[0];
	       if (STOCK_IDS[0] > 0)
		   STOCK_IDS[0]--;
	       else
		   STOCK_IDS[0] = 50;
	       while (FIELD[7][n + 1] < 52) {
		    FIELD[7][n] = FIELD[7][n + 1];
	            n++;
	       }
	       FIELD[7][n] = 52;
	   }
	   if (X > card_w * 3 + d) {
	       int suit = X / card_w - 3;
	       // Btw I use suit + 1 because STOCK_IDS[0] is X 
	       // and suits start only on STOCK_IDS[1]
	       if (STOCK_IDS[suit + 1] < 52) {
		   MOVING[0] = suit * 13 +  STOCK_IDS[suit + 1];
		   MOVING[13] = 8 + suit;
		   if (STOCK_IDS[suit + 1] == 0)
                       STOCK_IDS[suit + 1] = 52;
	           else
	               STOCK_IDS[suit + 1]--;
	       }
	   }
	}
    }
    if (!CLICKED && MOVING[0] != 52) {
	// Putting cards on their places after you moved them
	if ((Y > card_h + d) && (MOVING[0] != 52)) {
	   int i = X / card_w;
           int j = (Y - card_h - d) / lroundf(card_h / 3);
	   if ((FIELD[i][j - 1] < 52) && (FIELD[i][j] == 52) && (j > 0)) 
	       j--;
	   else 
	   if ((FIELD[i][j - 2] < 52) && (FIELD[i][j - 1] == 52) && (j > 1))
	       j -= 2;
	   if ((FIELD[i][j] < 52) && (MOVING[0] % 13 == 
				   FIELD[i][j] % 13 - 1) && 
	             (abs(FIELD[i][j] / 13 - MOVING[0] / 13) % 2 == 1)) {
	       n = 0;
	       while ((MOVING[n] < 52) && (n < 13)) {
		   FIELD[i][j + n + 1] = MOVING[n];
		   MOVING[n] = 52;
		   n++;
	       }
	       MOVING[13] = 52;
	   }
	   if ((FIELD[i][j] == 52) && (j < 3)) {
	       n = 0;
	       while ((MOVING[n] < 52) && (n < 13)) {
		   FIELD[i][n] = MOVING[n];
		   MOVING[n] = 52;
		   n++;
	       }
               MOVING[13] = 52;
	   }
	}
	// Putting cards in their suit places (A, B, C, D)
	if ((Y < card_h - d) && (MOVING[1] == 52) && (X > card_w * 3 + d)) {
            int suit = (X - card_w * 3) / card_w;
	    if (suit == MOVING[0] / 13) {
	        if ((STOCK_IDS[suit + 1] == 52) && (MOVING[0] % 13 == 0)) {
	            STOCK_IDS[suit + 1] = 0;
		    MOVING[0] = 52;
		    MOVING[13] = 52;
		}
		else if (STOCK_IDS[suit + 1] == MOVING[0] % 13 - 1) {
		    STOCK_IDS[suit + 1]++;
		    MOVING[0] = 52;
		    MOVING[13] = 52;
		}
		else
                    goto return_card;
	    }
	    else
		goto return_card; // |
	}                         // |
	else {                    // |
	    return_card: // <--------- we goto here
	    // If player tried to move cards in a bad location
	    // we return them on their previous places
	    n = 0;
	    // cards were taken from the 7 decks/piles
	    if (MOVING[13] < 7) {
		int j;
		for (j = 0; j < 52; j++)
		    if (FIELD[MOVING[13]][j] == 52)
			break;
	        while ((MOVING[n] < 52) && (n < 13)) {
                    FIELD[MOVING[13]][j + n] = MOVING[n];
		    MOVING[n] = 52;
                    n++;
	        }
	    }
	    // card was taken from the stock
	    if (MOVING[13] == 7) {
		if (FIELD[7][STOCK_IDS[0]] == 52) {
		    STOCK_IDS[0] = 0;
		}
		else
                    STOCK_IDS[0]++;
		for (int i = 50; i > STOCK_IDS[0]; i--)
	            FIELD[7][i] = FIELD[7][i - 1];
		FIELD[7][STOCK_IDS[0]] = MOVING[0];
		MOVING[0] = 52;
		MOVING[13] = 52;
	    }
	    // card was taken from ABCD
	    else {
		int suit = MOVING[13] - 8;
		MOVING[0] = 52;
		MOVING[13] = 52;
		if (STOCK_IDS[suit + 1] == 52)
		    STOCK_IDS[suit + 1] = 0;
		else
		    STOCK_IDS[suit + 1]++;
	    }
	}
    }
    // There was a bug, if the player clicked too fast gtk didnt
    // realise that the mouse button was released and it stayed
    // pressed eternally, so now if the player isnt moving anything
    // we unclick his button
    if (MOVING[0] == 52)
	CLICKED = 0;
    gtk_widget_queue_draw_area(p_window, 0, 0, WINDOW_W, WINDOW_H);
    return FALSE; 
}

gboolean CLICK(GtkWidget *widget, GdkEventButton *event, gpointer data) { 
	if (event->type == GDK_BUTTON_PRESS)
	    CLICKED = 1; 
	if (event->type == GDK_BUTTON_RELEASE)
	    CLICKED = 0; 
	ROTATED = 0;
        X = event->x;
	Y = event->y;
	gtk_widget_queue_draw_area(p_window, 0, 0, WINDOW_W, WINDOW_H);
	return TRUE;

}
gboolean MOTION(GtkWidget *widget, GdkEventMotion *event, gpointer data) { 
        X = event->x;
	Y = event->y;
	gtk_widget_queue_draw_area(p_window, 0, 0, WINDOW_W, WINDOW_H);
	return TRUE;
}


void clicked(GtkButton *button, GtkWidget pWindow) {
    if (!SHOWN) {
	area = gtk_drawing_area_new();
        p_window = gtk_window_new(GTK_WINDOW_POPUP);
        gtk_window_set_title(GTK_WINDOW(p_window), "Create DB");
        gtk_window_set_default_size(GTK_WINDOW(p_window), WINDOW_W, WINDOW_H);

        gtk_container_add(GTK_CONTAINER(p_window), area);
        
        gtk_widget_show_all(p_window);
        gtk_widget_show(area);
	gtk_window_set_keep_above(GTK_WINDOW(p_window), TRUE);
        gtk_window_set_decorated(GTK_WINDOW(p_window), FALSE);
	
	// gtk_widget_set_events(area, gtk_widget_get_events(area) |
			// GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK);
	gtk_widget_add_events(area, GDK_BUTTON_PRESS_MASK);
	gtk_widget_add_events(area, GDK_BUTTON_RELEASE_MASK);
	gtk_widget_add_events(area, GDK_BUTTON1_MOTION_MASK);
        g_signal_connect (G_OBJECT(area), "draw", 
                          G_CALLBACK(render), NULL);
        g_signal_connect (G_OBJECT(area), "button-press-event",
			  G_CALLBACK(CLICK), NULL);
        g_signal_connect (G_OBJECT(area), "button-release-event",
			  G_CALLBACK(CLICK), NULL);
        g_signal_connect (G_OBJECT(area), "motion-notify-event",
                          G_CALLBACK(MOTION), NULL);
        // g_signal_connect (G_OBJECT(area), "configure-event",
			  // G_CALLBACK(configure_cb), NULL);
	int x = SCREEN_W - WINDOW_W;
	int y = SCREEN_H - WINDOW_H - xfce_panel_plugin_get_size(plugins);
	gtk_window_move(GTK_WINDOW(p_window), x, y);
        while (gtk_events_pending ())
            gtk_main_iteration ();	

	SHOWN = 1;
    }
    else {
	gtk_widget_destroy(p_window);
	SHOWN = 0;
    }
}	


static void
constructor(XfcePanelPlugin *plugin) {
    for (int i = 0; i < 13; i ++)
	MOVING[i] = 52;
    srand(time(NULL));
    SHOWN = 0;
    //GtkWidget *image;
    GtkWidget *button;
    plugins = plugin;
    // GtkWidget *window;
    // GtkWidget *box;


    new_game();
    
    button = gtk_button_new_with_label("♠Q");
    // gtk_widget_set_size_request(button, 20, 16);
    gtk_widget_queue_draw(button);
    // button = gtk_button_new_from_icon_name("♠Q", GTK_ICON_SIZE_BUTTON);
    // window = gtk_scrolled_window_new(NULL, NULL);
    // gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(window), 16);
    // gtk_window_set_default_size(GTK_WINDOW(p_window), 16, 16);
    // box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    //image = gtk_image_new_from_file("/home/zubakker/G/PanelPlugin/CARD.png");
    
    // GdkPixbuf *pixbuf = gtk_image_get_pixbuf(GTK_IMAGE(image));
    // pixbuf = gdk_pixbuf_scale_simple(pixbuf, 15, 15, GDK_INTERP_BILINEAR);
    // gtk_image_set_from_pixbuf(GTK_IMAGE(image), pixbuf);

    //gtk_button_set_image(GTK_BUTTON(button), image);
    //gtk_button_set_image_position(GTK_BUTTON(button), GTK_POS_BOTTOM);

    // gtk_container_add(GTK_CONTAINER(box), button);
    // gtk_box_pack_start(GTK_BOX(box), button, TRUE, TRUE, 10);
    surface = gdk_window_create_similar_surface(GDK_WINDOW(p_window),
		    CAIRO_CONTENT_COLOR, WINDOW_W, WINDOW_H);
   
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    g_signal_connect(button, "clicked", G_CALLBACK(clicked), NULL);
    g_signal_connect (G_OBJECT (plugin), "size-changed",
                      G_CALLBACK (sample_size_changed), NULL);

    // gtk_container_add(GTK_CONTAINER(window), button);
    gtk_container_add(GTK_CONTAINER(plugin), button);



    gtk_widget_show(button);
    // gtk_widget_show(window);
    xfce_panel_plugin_set_expand (XFCE_PANEL_PLUGIN(plugin), FALSE); 
}


XFCE_PANEL_PLUGIN_REGISTER_INTERNAL(constructor);

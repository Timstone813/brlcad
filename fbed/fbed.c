/*
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647
*/
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <fcntl.h>
#include "./extern.h"

static struct
	{
	int	xbits, ybits;
	int	xorig, yorig;
	unsigned char bits[16*2];
	} cursor = {
		16, 16,
		8, 8,
#include "./cursorbits.h"
	};

#define JUMP		(40/zoom_factor)
#define CLR_LEN		12
#define Fgets_Bomb() \
		fb_log( "\%s\"(%d) EOF encountered prematurely.\n", \
				__FILE__, __LINE__ ); \
		return	0;

static Panel	panel;	      /* Current panel.				*/
static Point	bitpad;
static Rectangle current;     /* Current saved rectangle.		*/
static int	step;	      /* Current step size.			*/
static int	size_viewport;
static int	last_key;

_LOCAL_ int	do_Bitpad();
_LOCAL_ int	fb_Setup();
_LOCAL_ int	pars_Argv();
_LOCAL_ int	push_Macro();
#if STD_SIGNAL_DECLS
_LOCAL_ void	general_Handler();
#else
_LOCAL_ int	general_Handler();
#endif
_LOCAL_ void	init_Try();
_LOCAL_ void	fb_Paint();
_LOCAL_ void	fb_Wind();
_LOCAL_ void	get_Point();
_LOCAL_ void	clip_Rectangle();
_LOCAL_ void	get_Rectangle();
_LOCAL_ void	fill_Rectangle();
_LOCAL_ void	fix_Rectangle();
_LOCAL_ void	put_Fb_Panel();
_LOCAL_ RGBpixel	*pixel_Avg();

_LOCAL_ int	/* ^X  */ f_Exec_Function(),
		/* ^I  */ f_Nop(),
		/* ^H  */ f_Win_Lft(),
		/* ^J  */ f_Win_Dwn(),
		/* ^K  */ f_Win_Up(),
		/* ^L  */ f_Win_Rgt(),
		/* ^M  */ f_Reset_View(),
		/* ^R  */ f_Redraw(),
		/* ^Z  */ f_Stop(),
		/* ESC */ f_Iterations(),
		/* SP  */ f_Press(),
		/* ,   */ f_Dec_Brush_Size(),
		/* .   */ f_Inc_Brush_Size(),
		/* <   */ f_Dec_Step_Size(),
		/* >   */ f_Inc_Step_Size(),
		/* ?   */ f_Menu(),
		/* A   */ f_Start_Macro(),
		/* B   */ f_Bind_Macro_To_Key(),
		/* C   */ f_Crunch_Image(),
		/* D   */ f_Dump_FBC(),
		/* E   */ f_Erase_Fb(),
		/* F   */ f_Flip_Resolution(),
		/* G   */ f_Get_Panel(),
		/* H   */ f_Jump_Lft(),
		/* J   */ f_Jump_Dwn(),
		/* K   */ f_Jump_Up(),
		/* L   */ f_Jump_Rgt(),
		/* N   */ f_Name_Keyboard_Macro(),
		/* P   */ f_Put_Panel(),
		/* R   */ f_Restore_RLE(),
		/* S   */ f_Save_RLE(),
		/* T   */ f_Transliterate(),
		/* U   */ f_Write_Macros_To_File(),
		/* W   */ f_Fill_Panel(),
		/* X   */ f_Bind_Key_To_Key(),
		/* Y   */ f_Bind_Name_To_Key(),
		/* Z   */ f_Stop_Macro(),
		/* a   */ f_Enter_Macro_Definition(),
		/* b   */ f_Set_Rectangle(),
		/* c   */ f_Center_Window(),
		/* d   */ f_Draw_Line(),
		/* f   */ f_Rd_Font(),
		/* g   */ f_Set_Pixel(),
		/* h   */ f_Move_Lft(),
		/* i   */ f_Zoom_In(),
		/* j   */ f_Move_Dwn(),
		/* k   */ f_Move_Up(),
		/* l   */ f_Move_Rgt(),
		/* m   */ f_Status_Monitor(),
		/* o   */ f_Zoom_Out(),
		/* p   */ f_Key_Set_Pixel(),
		/* q   */ f_Quit(),
		/* r   */ f_Rd_Fb(),
		/* s   */ f_String(),
		/* u   */ f_Rd_Macros_From_File(),
		/* w   */ f_Put_Pixel(),
		/* x   */ f_Set_X_Pos(),
		/* y   */ f_Set_Y_Pos(),
		/* Unbound */ f_Exec_Macro();

Func_Tab func_tab[] =
	{
/* NUL */ f_Nop,			NULL,	"nop",
/* ^A  */ f_Nop,			NULL,	"nop",
/* ^B  */ f_Nop,			NULL,	"nop",
/* ^C  */ f_Nop,			NULL,	"nop",
/* ^D  */ f_Nop,			NULL,	"nop",
/* ^E  */ f_Nop,			NULL,	"nop",
/* ^F  */ f_Nop,			NULL,	"nop",
/* ^G  */ f_Nop,			NULL,	"nop",
/* ^H  */ f_Win_Lft,			NULL,	"move-window-left",
/* ^I  */ f_Nop,			NULL,	"nop",
/* ^J  */ f_Win_Dwn,			NULL,	"move-window-down",
/* ^K  */ f_Win_Up,			NULL,	"move-window-up",
/* ^L  */ f_Win_Rgt,			NULL,	"move-window-right",
/* ^M  */ f_Reset_View,		NULL,	"reset-view",
/* ^N  */ f_Nop,			NULL,	"nop",
/* ^O  */ f_Nop,			NULL,	"nop",
/* ^P  */ f_Nop,			NULL,	"nop",
/* ^Q  */ f_Nop,			NULL,	"nop",
/* ^R  */ f_Redraw,			NULL,	"redraw-tty-screen",
/* ^S  */ f_Nop,			NULL,	"nop",
/* ^T  */ f_Nop,			NULL,	"nop",
/* ^U  */ f_Nop,			NULL,	"nop",
/* ^V  */ f_Nop,			NULL,	"nop",
/* ^W  */ f_Nop,			NULL,	"nop",
/* ^X  */ f_Exec_Function,		NULL,	"execute-function-or-macro",
/* ^Y  */ f_Nop,			NULL,	"nop",
/* ^Z  */ f_Stop,			NULL,	"stop-program",
/* ESC */ f_Iterations,		NULL,	"argument-count",
/* FS  */ f_Nop,			NULL,	"nop",
/* GS  */ f_Nop,			NULL,	"nop",
/* RS  */ f_Nop,			NULL,	"nop",
/* US  */ f_Nop,			NULL,	"nop",
/* SP  */ f_Press,			NULL,	"pick-point",
/* !   */ f_Nop,			NULL,	"nop",
/* "   */ f_Nop,			NULL,	"nop",
/* #   */ f_Nop,			NULL,	"nop",
/* $   */ f_Nop,			NULL,	"nop",
/* %   */ f_Nop,			NULL,	"nop",
/* &   */ f_Nop,			NULL,	"nop",
/* `   */ f_Nop,			NULL,	"nop",
/* (   */ f_Nop,			NULL,	"nop",
/* )   */ f_Nop,			NULL,	"nop",
/* *   */ f_Nop,			NULL,	"nop",
/* +   */ f_Nop,			NULL,	"nop",
/* ,   */ f_Dec_Brush_Size,		NULL,	"decrement-brush-size",
/* -   */ f_Nop,			NULL,	"nop",
/* .   */ f_Inc_Brush_Size,		NULL,	"increment-brush-size",
/* /   */ f_Nop,			NULL,	"nop",
/* 0   */ f_Nop,			NULL,	"nop",
/* 1   */ f_Nop,			NULL,	"nop",
/* 2   */ f_Nop,			NULL,	"nop",
/* 3   */ f_Nop,			NULL,	"nop",
/* 4   */ f_Nop,			NULL,	"nop",
/* 5   */ f_Nop,			NULL,	"nop",
/* 6   */ f_Nop,			NULL,	"nop",
/* 7   */ f_Nop,			NULL,	"nop",
/* 8   */ f_Nop,			NULL,	"nop",
/* 9   */ f_Nop,			NULL,	"nop",
/* :   */ f_Nop,			NULL,	"nop",
/* ;   */ f_Nop,			NULL,	"nop",
/* <   */ f_Dec_Step_Size,		NULL,	"decrement-step-size",
/* =   */ f_Nop,			NULL,	"nop",
/* >   */ f_Inc_Step_Size,		NULL,	"increment-step-size",
/* ?   */ f_Menu,			NULL,	"print-bindings",
/* @   */ f_Nop,			NULL,	"nop",
/* A   */ f_Start_Macro,		NULL,	"start-macro-definition",
/* B   */ f_Bind_Macro_To_Key,		NULL,	"bind-macro-to-key",
/* C   */ f_Crunch_Image,		NULL,	"shrink-image-by-half",
/* D   */ f_Dump_FBC,			NULL,	"dump-fbc-registers",
/* E   */ f_Erase_Fb,			NULL,	"clear-framebuffer-memory",
/* F   */ f_Flip_Resolution,		NULL,	"flip-framebuffer-resolution",
/* G   */ f_Get_Panel,			NULL,	"get-current-rectangle",
/* H   */ f_Jump_Lft,			NULL,	"jump-cursor-left",
/* I   */ f_Nop,			NULL,	"nop",
/* J   */ f_Jump_Dwn,			NULL,	"jump-cursor-down",
/* K   */ f_Jump_Up,			NULL,	"jump-cursor-up",
/* L   */ f_Jump_Rgt,			NULL,	"jump-cursor-right",
/* M   */ f_Nop,			NULL,	"nop",
/* N   */ f_Name_Keyboard_Macro,	NULL,	"name-keyboard-macro",
/* O   */ f_Nop,			NULL,	"nop",
/* P   */ f_Put_Panel,			NULL,	"put-saved-rectangle",
/* Q   */ f_Nop,			NULL,	"nop",
/* R   */ f_Restore_RLE,		NULL,	"read-rle-fle",
/* S   */ f_Save_RLE,			NULL,	"write-rle-file",
/* T   */ f_Transliterate,		NULL,	"replace-pixel-current-rectangle",
/* U   */ f_Write_Macros_To_File,	NULL,	"write-macros-to-file",
/* V   */ f_Nop,			NULL,	"nop",
/* W   */ f_Fill_Panel,		NULL,	"fill-current-rectangle",
/* X   */ f_Bind_Key_To_Key,		NULL,	"bind-key-to-key",
/* Y   */ f_Bind_Name_To_Key,		NULL,	"bind-key-to-name",
/* Z   */ f_Stop_Macro,		NULL,	"stop-macro-definition",
/* [   */ f_Nop,			NULL,	"nop",
/* \   */ f_Nop,			NULL,	"nop",
/* ]   */ f_Nop,			NULL,	"nop",
/* ^   */ f_Nop,			NULL,	"nop",
/* _   */ f_Nop,			NULL,	"nop",
/* `   */ f_Nop,			NULL,	"nop",
/* a   */ f_Enter_Macro_Definition,	NULL,	"enter-macro-definition",
/* b   */ f_Set_Rectangle,		NULL,	"set-current-rectangle",
/* c   */ f_Center_Window,		NULL,	"window-center",
/* d   */ f_Draw_Line,			NULL,	"draw-line",
/* e   */ f_Nop,			NULL,	"nop",
/* f   */ f_Rd_Font,			NULL,	"read-font",
/* g   */ f_Set_Pixel,			NULL,	"set-paint-to-current-pixel",
/* h   */ f_Move_Lft,			NULL,	"move-cursor-left",
/* i   */ f_Zoom_In,			NULL,	"zoom-in",
/* j   */ f_Move_Dwn,			NULL,	"move-cursor-down",
/* k   */ f_Move_Up,			NULL,	"move-cursor-up",
/* l   */ f_Move_Rgt,			NULL,	"move-cursor-right",
/* m   */ f_Status_Monitor,		NULL,	"set-monitor",
/* n   */ f_Nop,			NULL,	"nop",
/* o   */ f_Zoom_Out,			NULL,	"zoom-out",
/* p   */ f_Key_Set_Pixel,		NULL,	"set-paint-from-key",
/* q   */ f_Quit,			NULL,	"quit",
/* r   */ f_Rd_Fb,			NULL,	"read-framebuffer",
/* s   */ f_String,			NULL,	"put-string",
/* t   */ f_Nop,			NULL,	"nop",
/* u   */ f_Rd_Macros_From_File,	NULL,	"read-macros-from-file",
/* v   */ f_Nop,			NULL,	"nop",
/* w   */ f_Put_Pixel,			NULL,	"put-pixel",
/* x   */ f_Set_X_Pos,			NULL,	"set-cursor-x-pos",
/* y   */ f_Set_Y_Pos,			NULL,	"set-cursor-y-pos",
/* z   */ f_Nop,			NULL,	"nop",
/* {   */ f_Nop,			NULL,	"nop",
/* |   */ f_Nop,			NULL,	"nop",
/* }   */ f_Nop,			NULL,	"nop",
/* ~   */ f_Nop,			NULL,	"nop",
/* DEL */ f_Nop,			NULL,	"nop"
};

static Func_Tab	*bindings[DEL+1];
static Func_Tab	*macro_entry = FT_NULL; /* Last keyboard macro defined.	*/

FBIO	*fbp;				/* Current framebuffer */
static int	cur_width = 512;

/*	m a i n ( )							*/
main( argc, argv )
char	*argv[];
	{
	if( ! pars_Argv( argc, argv ) )
		{
		prnt_Usage();
		return	1;
		}
	setbuf( stdout, malloc( BUFSIZ ) );	
	tty = isatty( 1 );
	if( ! InitTermCap( stdout ) )
		{
		(void) fprintf( stderr, "Could not initialize terminal.\n" );
		return	1;
		}
	init_Status();
	if( fb_Setup() == -1 )
		return	1;
	prnt_Status();
	fb_Wind();
	init_Tty();
	init_Try();
	get_Font( FONTNAME );
	{	static char	default_macro_file[MAX_LN];
		char		*home;
	if( (home = getenv( "HOME" )) != NULL )
		{
		(void) strcpy( default_macro_file, home );
		home = default_macro_file + strlen( default_macro_file );
		*home++ = '/';
		}
	else
		home = default_macro_file;
	(void) strcpy( home, ".fbed_macros" );
	(void) f_Rd_Macros_From_File( default_macro_file );
	}
#define CBREAK_MODE		/* Signals are generated from keyboard.	*/
#if defined( CBREAK_MODE )
	{	register int	sig;
	for( sig = 0; sig < NSIG; sig++ )
		if( signal( sig, general_Handler ) == SIG_IGN )
			(void) signal( sig, SIG_IGN );
	}
#endif
	(void) fb_flush( fbp );
	prnt_Prompt( "" );
	for( cread_buf[0] = NUL; ; )
		{	register int	button_press;
			register int	status_change = false;
		for( ; *cptr != NUL; )
			{
			do_Key_Cmd( (int) *cptr++, 1 );
			status_change = true;
			}
		if( cptr > cread_buf )
			*(cptr = cread_buf) = NUL;
		else
			{	int	c;
			if( reposition_cursor )
				{
				(void) fb_cursor(	fbp,
							1,
							cursor_pos.p_x,
							cursor_pos.p_y
							);
				status_change = true;
				reposition_cursor = false;
				}
			if( ! empty( tty_fd ) )
				{
				if( (c = get_Char()) != EOF )
					{
					do_Key_Cmd( c, 1 );
					status_change = true;
					}
				else	/* EOF detected.		*/
					(void) f_Quit( (char *) NULL );
				}
			}
		if(   !	reposition_cursor
		    &&	(button_press = get_Mouse_Pos( &cursor_pos ))
		    	!= -1
			)
			status_change = true;
		if( status_change )
			{
			(void) fb_flush( fbp );
			if( report_status )
				prnt_Status();
			}
		}
	}

_LOCAL_ void
fill_Rectangle( rectp, pixelp )
register Rectangle	*rectp;
register RGBpixel	*pixelp;
	{	register int	btm = rectp->r_origin.p_y;
		register int	top = rectp->r_corner.p_y;
		register int	rgt = rectp->r_corner.p_x;
		int		lft = rectp->r_origin.p_x;
#ifdef sgi /* More efficient on IRIS.					*/
	if( top - btm < 10 || rgt - lft < 10 )
		{
		for( ; btm <= top; btm++ )
			{	register int	x;
			for( x = lft; x <= rgt; x++ )
				(void) fb_write( fbp, x, btm, pixelp, 1 );
			}
		return;
		}
#endif
	for( ; btm <= top; btm++ )
		{	register int	x = lft;
		(void) fb_seek( fbp, x, btm );
		for( ; x <= rgt; x++ )
			FB_WPIXEL( fbp, *pixelp );
		}
	(void) fb_flush( fbp );
	return;
	}

/*	i n i t _ T r y ( )
	Initialize "try" tree of function names, with key bindings.
 */
_LOCAL_ void
init_Try()
	{	register int	key;
		register int	nop_key = EOF;
	/* Add all functions except NOP to tree.			*/
	for( key = NUL; key <= DEL; key++ )
		{
		bindings[key] = &func_tab[key];
		if( bindings[key]->f_func != f_Nop )
			add_Try( bindings[key], bindings[key]->f_name, &try_rootp );
		else
		if( nop_key == EOF )
			/* First key bound to NOP.			*/
			nop_key = key;
		}
	/* Add the NOP function to the tree once, although it may be bound
		to many keys.
	 */
	add_Try( &func_tab[nop_key], func_tab[nop_key].f_name, &try_rootp );
	return;
	}

_LOCAL_ int
push_Macro( buf )
char	*buf;
	{	register int	curlen = strlen( cptr );
		register int	buflen = strlen( buf );
	if( curlen + buflen > MACROBUFSZ - 1 )
		{
		fb_log( "Macro buffer would overflow.\n" );
		return	0;
		}
	(void) strcpy( macro_buf, cptr );
	(void) strncpy( cread_buf, buf, buflen ); /* Don't copy NUL.	*/
	(void) strcpy( cread_buf+buflen, macro_buf );
	cptr = cread_buf;
	return	1;
	}

/*	d o _ K e y _ C m d ( )						*/
void
do_Key_Cmd( key, n )
register int	key;
register int	n;
	{
	last_key = key;
	if( *cptr == NUL )
		{
		/*prnt_Prompt( "" );*/
		prnt_Event( "" );
		}
	if( remembering )
		{
		if( bindings[key]->f_func != f_Exec_Macro )
			{
			*macro_ptr++ = key;
			*macro_ptr = NUL;
			}
		}
	step = gain;
	while( n-- > 0 )
		{
		/* For now, ignore return values;
			-1 for illegal command.
			 0 for failed command.
		 	 1 for success.
		 */
		(void) (*bindings[key]->f_func)( bindings[key]->f_buff );
		}
	return;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Nop( buf )
char	*buf;
	{
	fb_log( "Unbound(%s).\n", char_To_String( last_key ) );
	putchar( BEL );
	return	-1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Win_Lft( buf ) /* Move window left.					*/
char	*buf;
	{
	windo_center.p_x += gain;
	reposition_cursor = true;
	return	fb_window( fbp, windo_center.p_x, windo_center.p_y ) != -1 ? 1 : 0;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Win_Dwn( buf ) /* Move window down.					*/
char	*buf;
	{
	windo_center.p_y -= gain;
	reposition_cursor = true;
	return	fb_window( fbp, windo_center.p_x, windo_center.p_y ) != -1 ? 1 : 0;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Win_Up( buf ) /* Move window up.					*/
char	*buf;
	{
	windo_center.p_y += gain;
	reposition_cursor = true;
	return	fb_window( fbp, windo_center.p_x, windo_center.p_y ) != -1 ? 1 : 0;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Win_Rgt( buf ) /* Move window right.					*/
char	*buf;
	{
	windo_center.p_x -= gain;
	reposition_cursor = true;
	return	fb_window( fbp, windo_center.p_x, windo_center.p_y ) != -1 ? 1 : 0;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Reset_View( buf ) /* Restore normal view.				*/
char	*buf;
	{
	cursor_pos.p_x = image_center.p_x;
	cursor_pos.p_y = image_center.p_y;
	size_viewport = fb_getwidth(fbp);
	fb_Wind();
	reposition_cursor = true;
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Redraw( buf ) /* Redraw screen.					*/
char	*buf;
	{
	init_Status();
	prnt_Status();
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Stop( buf ) /* Stop program.						*/
char	*buf;
	{	int	pid = getpid();
		int	sig;
#ifdef BSD
	sig = SIGSTOP;
#else
	sig = 17;
#endif
	prnt_Event( "[%d] stopped.", pid );
	restore_Tty();
	if( kill( pid, sig ) == -1 )
		{	extern int	errno;
		perror( "(fbed.c) kill" );
		exit( errno );
		}
	init_Tty();
	init_Status();
	prnt_Event( "[%d] foreground.", pid );
	prnt_Status();
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Exec_Function( buf )
char	*buf;
	{	Func_Tab	*ftbl;
		static char	name[MAX_LN];
	if( (ftbl = get_Func_Name( name, MAX_LN, ": " )) == FT_NULL )
		return	0;
	else
	if( strcmp( ftbl->f_name, name ) == 0 )
		return	(*ftbl->f_func)( ftbl->f_buff );
	else
		{
		fb_log( "I seem to have lost my bindings.\n" );
		return	0;
		}
	}

#define MAX_DIGITS	4
_LOCAL_ int
/*ARGSUSED*/
f_Iterations( buf ) /* Specify number of iterations of next command.	*/
char	*buf;
	{	char		iterate_buf[MAX_DIGITS+1];
		int		iterate;
		register int	c, i;
	if( remembering )
		/* Clobber "f_Iterations()" key-stroke.		*/
		*--macro_ptr = NUL;
	prnt_Prompt( "M-" );
	for( i = 0; i < MAX_DIGITS && isdigit( c = get_Char() ); i++ )
		{
		iterate_buf[i] = c;
		(void) putchar( c );
		(void) fflush( stdout );
		}
	if( i == MAX_DIGITS )
		c = get_Char();
	iterate_buf[i] = NUL;
	(void) putchar( ':' );
	(void) fflush( stdout );
	if( sscanf( iterate_buf, "%d", &iterate ) != 1 )
		{
		fb_log( "Iterations not set.\n" );
		return	0;
		}
	do_Key_Cmd( c, iterate );
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Press( buf )
char	*buf;
	{
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Dec_Brush_Size( buf ) /* Decrement brush size.			*/
char	*buf;
	{
	if( brush_sz > 0 )
		brush_sz -= 2;
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Inc_Brush_Size( buf ) /* Increment brush size.			*/
char	*buf;
	{
	if( brush_sz < size_viewport )
		brush_sz += 2;
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Menu( buf ) /* Print menu.						*/
char	*buf;
	{	register int	lines =	(PROMPT_LINE-BOTTOM_STATUS_AREA)-1;
		register int	done = false;
		register int	key;
	for( key = NUL; key <= DEL && ! done; )
		{	register int	j;
		for( j = 0; key <= DEL && j < lines; key++ )
			{
			if( bindings[key]->f_func != f_Nop )
				{
				prnt_Scroll(	" '%s'\t%s",
						char_To_String( key ),
						bindings[key]->f_name
						);
				j++;
				}
			}
		/* See if there is any more output.			*/
		for( j = key; j <= DEL && bindings[j]->f_func == f_Nop; j++ )
			;
		if( j <= DEL )
			{
			SetStandout();
			prnt_Prompt( "-- More -- " );
			ClrStandout();
			(void) fflush( stdout );
			switch( *cptr != NUL ? *cptr++ : get_Char() )
				{
			case 'q' :
			case 'n' :
				done = true;
				break;
			case LF :
			case CR :
				lines = 1;
				break;
			default :
				lines =	(PROMPT_LINE-BOTTOM_STATUS_AREA)-1;
				break;
				}
			prnt_Prompt( "" );
			}
		}
	(void) fflush( stdout );
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Dec_Step_Size( buf ) /* Decrement gain on move operations.		*/
char	*buf;
	{
	if( gain > 1 )
		gain--;
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Inc_Step_Size( buf ) /* Increment gain on move operations.		*/
char	*buf;
	{
	if( gain < size_viewport )
		gain++;
	return	1;
	}

_LOCAL_ int
f_Exec_Macro( buf )
char	*buf;
	{
	push_Macro( buf );
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Rd_Macros_From_File( buf )
char	*buf;
	{	register FILE	*macro_fp;
		register int	nread = 1, room;
		char		scratch[MAX_LN];
	if( buf != NULL )
		{
		if( (macro_fp = fopen( buf, "r" )) == NULL )
			return	1;
		else
			prnt_Event( "Reading macros from file \"%s\".", buf );
		}
	else
		{
		if( ! get_Input( scratch, MAX_LN, "Read macros from file : " ) )
			return	0;
		if( (macro_fp = fopen( scratch, "r" )) == NULL )
			{
			fb_log( "Can't open \"%s\" for reading.\n", scratch );
			return	0;
			}
		}
	/* Read and execute functions from file.			*/
	for( ; nread > 0 ; )
		{
		room = MACROBUFSZ - strlen( cread_buf );
		nread = fread( cptr, (int) sizeof(char), room , macro_fp );
		cread_buf[nread] = NUL;
		for( cptr = cread_buf; *cptr != NUL; )
			do_Key_Cmd( (int) *cptr++, 1 );
		*(cptr = cread_buf) = NUL;
		}
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Write_Macros_To_File( buf )
char	*buf;
	{	static char	macro_file[MAX_LN];
		register FILE	*macro_fp;
		register int	key;
	if( ! get_Input( macro_file, MAX_LN, "Write macros to file : " ) )
		return	0;
	if( (macro_fp = fopen( macro_file, "w" )) == NULL )
		{
		fb_log( "Can't open \"%s\" for writing.\n", macro_file );
		return	0;
		}
	for( key = NUL+1; key <= DEL; key++ )
		{
		if( bindings[key] != FT_NULL )
			{
			if( bindings[key]->f_func == f_Exec_Macro )
				{
				(void) putc( Ctrl('X'), macro_fp );
				(void) fprintf(	macro_fp,
						"enter-macro-definition\n"
						);
				/* Output macro definition.			*/
				for(	macro_ptr = bindings[key]->f_buff;
					*macro_ptr != NUL;
					macro_ptr++
					)
					(void) putc( *macro_ptr, macro_fp );
				(void) putc( 'Z', macro_fp ); /* Mark end macro.*/
				(void) putc( Ctrl('X'), macro_fp );
				(void) fprintf( macro_fp, "name-keyboard-macro\n" );
				/* Output macro name, new-line terminated.	*/
				(void) fprintf(	macro_fp,
						"%s\n", bindings[key]->f_name
						);
				(void) putc( Ctrl('X'), macro_fp );
				(void) fprintf( macro_fp, "bind-macro-to-key\n" );
				/* Output key binding, new-line terminated.	*/
				if( key < SP || key == DEL )
					/* Escape control characters.		*/
					(void) putc( Ctrl('V'), macro_fp );
				(void) putc( key, macro_fp );
				(void) putc( '\n', macro_fp );
				}
			/* Write out current key binding.		*/
			if( key == Ctrl('X') || key == '@' )
				continue;
			(void) putc( Ctrl('X'), macro_fp );
			(void) fprintf( macro_fp, "bind-key-to-name\n" );
			if( key < SP || key == DEL )
				(void) putc( Ctrl('V'), macro_fp );
			(void) putc( key, macro_fp );
			(void) putc( '\n', macro_fp );
			(void) fprintf( macro_fp, "%s\n", bindings[key]->f_name );
			}
		}
	(void) fflush( macro_fp );
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Start_Macro( buf )
char	*buf;
	{
	if( remembering )
		{
		fb_log( "I am already remembering.\n" );
		*--macro_ptr = NUL;
		return	0;
		}
	*(macro_ptr = macro_buf) = NUL;
	remembering = true;
	prnt_Event( "Remembering..." );
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Bind_Macro_To_Key( buf )
char	*buf;
	{	char		key[2];
	if( macro_entry == FT_NULL )
		{
		fb_log( "Define macro first.\n" );
		return	0;
		}
	if( ! get_Input( key, 2, "Bind macro to key : " ) )
		return	0;
	if( key[0] == Ctrl('X') || key[0] == '@' )
		{
		(void) putchar( BEL );
		fb_log(	"It is not permitted to change '%s' binding.\n",
				char_To_String( (int) key[0] )
				);
		return	0;
		}
	bindings[key[0]] = macro_entry;
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Name_Keyboard_Macro( buf )
char	*buf;
	{	static char	macro_name[MAX_LN];
	if( macro_entry == FT_NULL )
		{
		fb_log( "Define macro first.\n" );
		return	0;
		}
	if( ! get_Input( macro_name, MAX_LN, "Name keyboard macro : " ) )
		return	0;
	macro_entry->f_name = malloc( (unsigned) strlen( macro_name )+1 );
	if( macro_entry->f_name == NULL )
		{
		Malloc_Bomb();
		}
	(void) strcpy( macro_entry->f_name, macro_name );
	add_Try( macro_entry, macro_entry->f_name, &try_rootp );
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Crunch_Image( buf ) /* Average image to half its size.		*/
char	*buf;
	{	char	answer[2];
		register int	x, y;
		register RGBpixel	*p1, *p2;
	if( ! get_Input( answer, 2, "Crunch image [n=no] ? " ) )
		return	0;
	if( answer[0] == 'n' )
		return	1;
	if( (p1 = (RGBpixel *) malloc( sizeof(RGBpixel) * fb_getwidth(fbp) ))
		== RGBPIXEL_NULL
		)
		{
		Malloc_Bomb();
		}
	if( (p2 = (RGBpixel *) malloc( sizeof(RGBpixel) * fb_getwidth(fbp) ))
		== RGBPIXEL_NULL
		)
		{
		Malloc_Bomb();
		}
	for( y = 0; y < fb_getheight(fbp); y += 2 )
		{	register RGBpixel	*p_avg;
		fb_read( fbp, 0, y, p1, fb_getwidth(fbp));
		fb_read( fbp, 0, y+1, p2, fb_getwidth(fbp) );
		for( x = 0; x < fb_getwidth(fbp); x +=2 )
			{
			p_avg = pixel_Avg(	p1+x,
						p1+x+1,
						p2+x,
						p2+x+1
						);
			COPYRGB( p1[x/2], *p_avg );
			}
		fb_write( fbp, 0, y/2, p1, fb_getwidth(fbp)/2 );
		}
	free( (char *) p1 );
	free( (char *) p2 );
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Draw_Line( buf )
char	*buf;
	{	Rectangle	lineseg;
		register int	major;
		register int	minor;
		register int	xsign;
		register int	ysign;
		register int	error;
		register int	x;
		register int	de;
		int		xmajor;

	get_Point( "Pick starting point of line", &lineseg.r_origin );
	get_Point( "Pick ending point of line", &lineseg.r_corner );

	/* Below is a implementation of Bresenham's algorithm written
		by Douglas A. Gwyn of BRL, as suggested by Newman and
		Sproull, "Principles of Interactive Computer Graphics",
		Second Edition, pages 25-26.				*/
	/* Arrange for X coordinate to increase from start point to
		end point.						*/
	if( lineseg.r_origin.p_x > lineseg.r_corner.p_x )
		{	Point	temp;
		temp = lineseg.r_origin;
		lineseg.r_origin = lineseg.r_corner;
		lineseg.r_corner = temp;
		}
	major = lineseg.r_corner.p_x - lineseg.r_origin.p_x;
	xsign = major ? 1 : 0;
	minor = lineseg.r_corner.p_y - lineseg.r_origin.p_y;
	ysign = minor ? (minor > 0 ? 1 : -1) : 0;
	if( ysign < 0 )
		minor = -minor;
	/* If X is not really major, correct the assignments.		*/
	if( ! (xmajor = minor <= major) )
		{	register int	temp = minor;
		minor = major;
		major = temp;
		}

	error = major / 2 - minor; /* Initial DDA error.		*/
	de = major - minor;
	for( x = lineseg.r_origin.p_x; x <= lineseg.r_corner.p_x; )
		{
		(void) fb_write(	fbp,
					x, lineseg.r_origin.p_y, 
					(RGBpixel *) paint,
					1
					);
		if( major-- == 0 ) /* Done!				*/
			return	1;
		if( error < 0 )	 /* Advance major and minor.		*/
			{
			x += xsign;
			lineseg.r_origin.p_y += ysign;
			error += de;
			}
		else
			{		/* Advance major only.		*/
			if( xmajor )	/* X is major direction.	*/
				x++;
			else		/* Y is major direction.	*/
				lineseg.r_origin.p_y += ysign;
			error -= minor;
			}
		}
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Fill_Panel( buf ) /* Fill current rectangle with "paint" color.	*/
char	*buf;
	{
	fill_Rectangle( &current, (RGBpixel *) paint );
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Bind_Key_To_Key( buf ) /* Bind new key to same function as old key.	*/
char	*buf;
	{	char	old_key[2], new_key[2];
	if( ! get_Input( new_key, 2, "Bind new key : " ) )
		return	0;
	if( new_key[0] == Ctrl('X') || new_key[0] == '@' )
		{
		(void) putchar( BEL );
		fb_log(	"It is not permitted to change '%s' binding.\n",
				char_To_String( (int) new_key[0] )
				);
		return	0;
		}
	if( ! get_Input( old_key, 2, "To function bound to key : " ) )
		return	0;
	bindings[new_key[0]] = bindings[old_key[0]];
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Bind_Name_To_Key( buf ) /* Bind key to function or macro.		*/
char	*buf;
	{	char	key[2];
		static char	name[MAX_LN];
		Func_Tab	*ftbl;
	if( ! get_Input( key, 2, "Bind key : " ) )
		return	0;
	if( key[0] == Ctrl('X') || key[0] == '@' )
		{
		(void) putchar( BEL );
		fb_log(	"It is not permitted to change '%s' binding.\n",
				char_To_String( (int) key[0] )
				);
		return	0;
		}
	if( (ftbl = get_Func_Name( name, MAX_LN, "To function/macro name : " ))
		== FT_NULL
		)
		return	0;
	if( strcmp( ftbl->f_name, name ) == 0 )
		{
		/* Key is still bound to this function/macro.		*/
		bindings[key[0]] = ftbl;
		return	1;
		}
	else
		{
		fb_log( "I seem to have lost my bindings.\n" );
		return	0;
		}
	}

_LOCAL_ int
/*ARGSUSED*/
f_Dump_FBC( buf ) /* Dump frame buffer controller (FBC) registers.	*/
char	*buf;
	{
	fb_log( "Dump_FBC unimplemented.\n" );
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Erase_Fb( buf ) /* Erase (clear) framebuffer.			*/
char	*buf;
	{	char	answer[2];
	if( ! get_Input( answer, 2, "Clear framebuffer [n=no] ? " ) )
		return	0;
	if( answer[0] != 'n' )
		{
		(void) fb_clear(fbp, RGBPIXEL_NULL);
		}
	reposition_cursor = true;
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Flip_Resolution( buf ) /* Flip framebuffer resolution.		*/
char	*buf;
	{	char	answer[2];
		int	is_hires = fb_getwidth(fbp) > 512;
	if( ! get_Input(	answer,
				2,
				is_hires ?
				"Flip framebuffer to low res [n=no] ? " :
				"Flip framebuffer to high res [n=no] ? "
				)
		)
		return	0;
	if( answer[0] == 'n' )
		return	1;
	(void) fb_cursor( fbp, 0, 0, 0 );	/* off */
	if( fb_close( fbp ) == -1 )
		return	0;
	cur_width = is_hires ? 512 : 1024;
	if( fb_Setup() == -1 )
		exit( 1 );
	reposition_cursor = true;
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Get_Panel( buf ) /* Grab panel from framebuffer.			*/
char	*buf;
	{
	if( panel.n_buf != (RGBpixel *) NULL )
		free( (char *) panel.n_buf );
	prnt_Rectangle(	"Storing rectangle", &current );
	panel.n_buf = get_Fb_Panel( &current );
	panel.n_wid  = current.r_corner.p_x - current.r_origin.p_x;
	panel.n_hgt = current.r_corner.p_y - current.r_origin.p_y;
	fb_log( "Rectangle saved.\n" );
	return	1; 
	}

_LOCAL_ int
/*ARGSUSED*/
f_Jump_Lft( buf ) /* Move cursor left (big steps).			*/
char	*buf;
	{
	if( cursor_pos.p_x >= JUMP )
		cursor_pos.p_x -= JUMP;
	else
		cursor_pos.p_x = 0;
	reposition_cursor = true;
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Jump_Dwn( buf ) /* Move cursor down.					*/
char	*buf;
	{
	if( cursor_pos.p_y >= JUMP )
		cursor_pos.p_y -= JUMP;
	else
		cursor_pos.p_y = 0;
	reposition_cursor = true;
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Jump_Up( buf ) /* Move cursor up.					*/
char	*buf;
	{
	if( cursor_pos.p_y < fb_getheight(fbp) - JUMP )
		cursor_pos.p_y += JUMP;
	else
		cursor_pos.p_y = fb_getheight(fbp) - 1;
	reposition_cursor = true;
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Jump_Rgt( buf ) /* Move cursor right.				*/
char	*buf;
	{
	if( cursor_pos.p_x <= fb_getwidth(fbp) - JUMP )
		cursor_pos.p_x += JUMP;
	else
		cursor_pos.p_x = fb_getwidth(fbp) - 1;
	reposition_cursor = true;
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Put_Panel( buf ) /* Put grabbed panel to framebuffer.		*/
char	*buf;
	{
	if( panel.n_buf == (RGBpixel *) NULL )
		{
		fb_log( "You must use \"get-current-rectangle\" first.\n" );
		return	0;
		}
	get_Point( "Pick lower-left corner of panel", &current.r_origin );
	current.r_corner.p_x = current.r_origin.p_x + panel.n_wid;
	current.r_corner.p_y = current.r_origin.p_y + panel.n_hgt;
	put_Fb_Panel( &current, panel.n_buf );
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Restore_RLE( buf ) /* Restore Run-Length Encoded image.		*/
char	*buf;
	{	static char	rle_file_nm[MAX_LN];
		static FILE	*rle_fp;
		static char	*args[] =
			{
			"rle-fb", rle_file_nm, NULL
			};
	if( ! get_Input( rle_file_nm, MAX_LN, "Enter RLE file name : " ) )
		return	0;
	if( rle_file_nm[0] == NUL )
		{
		fb_log( "No default.\n" );
		return	0;
		}
	else
	if( (rle_fp = fopen( rle_file_nm, "r" )) == NULL )
		{
		fb_log( "Can't open \"%s\".\n", rle_file_nm );
		return	0;
		}
	prnt_Event( "Decoding \"%s\".", rle_file_nm );
	(void) fb_cursor( fbp, 0, 0, 0 );	/* off */
	reposition_cursor = true;
	if( fb_close( fbp ) == -1 )
		{
		(void) fclose( rle_fp );
		return	0;
		}
	(void) exec_Shell( args );
	if( fb_Setup() == -1 )
		exit( 1 );
	(void) fclose( rle_fp );
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Save_RLE( buf ) /* Save framebuffer image with Run-Length Encoding.	*/
char	*buf;
	{	static char	rle_file_nm[MAX_LN];
		static char	*args[4] =
			{
			"fb-rle", rle_file_nm, NULL, NULL
			};
	if( fb_getwidth(fbp) == 1024 )
		{
		args[1] = "-h";
		args[2] = rle_file_nm;
		}
	if( ! get_Input( rle_file_nm, MAX_LN, "Enter RLE file name : " ) )
		return	0;
	if( rle_file_nm[0] == NUL )
		{
		fb_log( "No default.\n" );
		return	0;
		}
	if( access( rle_file_nm, 0 ) == 0 )
		{	char	answer[2];
			char	question[MAX_LN+32];
		(void) sprintf( question,
				"File \"%s\" exists, remove [n=no] ? ",
				rle_file_nm
				); 
		if( ! get_Input( answer, 2, question ) )
			return	0;
		if( answer[0] == 'n' )
			return	0;
		(void) unlink( rle_file_nm );
		}
	prnt_Event( "Encoding \"%s\".", rle_file_nm );
	if( fb_close( fbp ) == -1 )
		return	0;
	if( exec_Shell( args ) == 0 )
		fb_log( "Image saved in \"%s\".\n", rle_file_nm );
	else
		fb_log( "Image not saved.\n" );
	if( fb_Setup() == -1 )
		exit( 1 );
	reposition_cursor = true;
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Transliterate( buf ) /* Transliterate pixels of color1 to target color2.*/
char	*buf;
	{	static char	old_color[CLR_LEN], new_color[CLR_LEN];
		static int	red, grn, blu;
		RGBpixel	old, new, cur;
		register int	x, y;
		register int	lft, rgt, top, btm;
	lft = current.r_origin.p_x;
	rgt = current.r_corner.p_x;
	top = current.r_origin.p_y;
	btm = current.r_corner.p_y;
	if( ! get_Input( old_color, CLR_LEN, "Enter old pixel color [r g b] : " ) )
		return	0;
	if(	sscanf( old_color, "%d %d %d", &red, &grn, &blu ) == 3
	    &&	red >= 0 && red <= 255
	    &&	grn >= 0 && grn <= 255
	    &&	blu >= 0 && blu <= 255
		)
		{
		old[RED] = red;
		old[GRN] = grn;
		old[BLU] = blu;
		}
	else
		{
		fb_log( "You must enter 3 numbers (0..255).\n" );
		return	0;
		}
	if( ! get_Input( new_color, CLR_LEN, "Enter new pixel color [r g b] : " ) )
		return	0;
	if(	sscanf( new_color, "%d %d %d", &red, &grn, &blu ) == 3
	    &&	red >= 0 && red <= 255
	    &&	grn >= 0 && grn <= 255
	    &&	blu >= 0 && blu <= 255
		)
		{
		new[RED] = red;
		new[GRN] = grn;
		new[BLU] = blu;
		}
	else
		{
		fb_log( "You must enter 3 numbers (0..255).\n" );
		return	0;
		}
	for( y = top; y <= btm ; y++ )
		{
		x = lft;
		(void) fb_seek( fbp, x, y );
		for( ; x <= rgt; x++ )
			{
			(void) fb_rpixel( fbp, cur );
			if(	cur[RED] == old[RED]
			    &&	cur[GRN] == old[GRN]
			    &&	cur[BLU] == old[BLU]
				)
				{
				(void) fb_seek( fbp, x, y );
				FB_WPIXEL( fbp, new );
				}
			}
		}
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Stop_Macro( buf )
char	*buf;
	{
	if( ! remembering )
		{
		fb_log( "I was not remembering.\n" );
		return	0;
		}
	remembering = false;
	/* Clobber "f_Stop_Macro()" key-stroke or "f_Exec_Function()
		followed by "stop-macro-definition".
	 */
	if( *--macro_ptr == '\n' )
		while( *--macro_ptr != Ctrl('X') )
			*macro_ptr = NUL;
	*macro_ptr = NUL;
	macro_ptr = macro_buf;
	macro_entry = (Func_Tab *) malloc( sizeof(Func_Tab) );
	if( macro_entry == FT_NULL )
		{
		Malloc_Bomb();
		}
	macro_entry->f_func = f_Exec_Macro;
	macro_entry->f_buff = malloc( (unsigned) strlen( macro_buf )+1 );
	if( macro_entry->f_buff == NULL )
		{
		Malloc_Bomb();
		}
	(void) strcpy( macro_entry->f_buff, macro_buf );
	fb_log( "Keyboard macro defined.\n" );
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Enter_Macro_Definition( buf )
char	*buf;
	{	register int	interactive = *cptr == NUL;
	macro_ptr = macro_buf;
	*macro_ptr = NUL;
	if( interactive )
		{
		prnt_Prompt( "Enter macro definition : " );
		while( (*macro_ptr++ = get_Char()) != 'Z' )
			;
		}
	else
		while( (*macro_ptr++ = *cptr++) != 'Z' )
			;
	/* Clobber macro terminator.					*/
	*--macro_ptr = NUL;
	macro_ptr = macro_buf;
	macro_entry = (Func_Tab *) malloc( sizeof(Func_Tab) );
	if( macro_entry == FT_NULL )
		{
		Malloc_Bomb();
		}
	macro_entry->f_func = f_Exec_Macro;
	macro_entry->f_buff = malloc( (unsigned) strlen( macro_buf )+1 );
	if( macro_entry->f_buff == NULL )
		{
		Malloc_Bomb();
		}
	(void) strcpy( macro_entry->f_buff, macro_buf );
	if( interactive )
		fb_log( "Keyboard macro defined.\n" );
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Set_Rectangle( buf ) /* Set current rectangle.			*/
char	*buf;
	{
	get_Rectangle( "rectangle", &current );
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Center_Window( buf ) /* Center window around cursor.			*/
char	*buf;
	{
	fb_Wind();
	reposition_cursor = true;
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Rd_Font( buf ) /* Set current font.				*/
char	*buf;
	{	static char	fontname[FONTNAMESZ];
	if( ! get_Input( fontname, FONTNAMESZ, "Enter font name : " ) )
		return	0;
	get_Font( fontname );
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Set_Pixel( buf ) /* Set "paint" pixel color.				*/
char	*buf;
	{
	fb_Get_Pixel( paint );
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Move_Lft( buf ) /* Move cursor left.					*/
char	*buf;
	{
	if( cursor_pos.p_x >= step )
		cursor_pos.p_x -= step;
	else
		cursor_pos.p_x = 0;
	reposition_cursor = true;
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Zoom_In( buf ) /* Halve window size.					*/
char	*buf;
	{
	if( size_viewport > fb_getwidth(fbp) / 16 )
		{
		size_viewport /= 2;
		fb_Wind();
		reposition_cursor = true;
		}
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Move_Dwn( buf ) /* Move cursor down.					*/
char	*buf;
	{
	if( cursor_pos.p_y >= step )
		cursor_pos.p_y -= step;
	else
		cursor_pos.p_y = 0;
	reposition_cursor = true;
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Move_Up( buf ) /* Move cursor up.					*/
char	*buf;
	{
	if( cursor_pos.p_y <= fb_getheight(fbp) - step )
		cursor_pos.p_y += step;
	else
		cursor_pos.p_y = fb_getheight(fbp) - 1;
	reposition_cursor = true;
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Move_Rgt( buf ) /* Move cursor right.				*/
char	*buf;
	{
	if( cursor_pos.p_x <= fb_getwidth(fbp) - step )
		cursor_pos.p_x += step;
	else
		cursor_pos.p_x = fb_getwidth(fbp) - 1;
	reposition_cursor = true;
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Status_Monitor( buf ) /* Toggle status monitoring.			*/
char	*buf;
	{
	Toggle( report_status );
	if( report_status )
		init_Status();
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Zoom_Out( buf ) /* Double window size.				*/
char	*buf;
	{
	if( size_viewport < fb_getwidth(fbp) )
		{
		size_viewport *= 2;
		fb_Wind();
		reposition_cursor = true;
		}
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Key_Set_Pixel( buf ) /* User types in paint color.			*/
char	*buf;
	{	static char	color[CLR_LEN];
		static int	red, grn, blu;
	if( ! get_Input( color, CLR_LEN, "Enter color [r g b] : " ) )
		return	0;
	if(	sscanf( color, "%d %d %d", &red, &grn, &blu ) == 3
	    &&	red >= 0 && red <= 255
	    &&	grn >= 0 && grn <= 255
	    &&	blu >= 0 && blu <= 255
		)
		{
		paint[RED] = red;
		paint[GRN] = grn;
		paint[BLU] = blu;
		}
	else
		{
		fb_log( "You must enter 3 numbers (0..255).\n" );
		return	0;
		}
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Quit( buf )
char	*buf;
	{
	prnt_Event( "Bye..." );
	restore_Tty();
	exit( 0 );
	/*NOTREACHED*/
	}

_LOCAL_ int
/*ARGSUSED*/
f_Rd_Fb( buf ) /* Read frame buffer image from file.			*/
char	*buf;
	{	static char	image[MAX_LN];
		static FBIO	*imp;
	if( ! get_Input( image, MAX_LN, "Enter framebuffer name : " ) )
		return	0;
	if( image[0] == NUL )
		{
		fb_log( "No default.\n" );
		return	0;
		}
	else
	if( (imp = fb_open( image, 512, 512 )) == FBIO_NULL )	/* XXX */
		{
		fb_log(	"Can't open \"%s\" for reading.\n", image );
		return	0;
		}
	prnt_Event( "Reading \"%s\".", image );
	(void) fb_cursor( fbp, 0, 0, 0 );	/* off */
	reposition_cursor = true;
	{	register int	y;
	for( y = 0; y < fb_getheight( imp ); y++ )
		{	RGBpixel	scanbuf[1024];
		if( fb_read( imp, 0, y, scanbuf, fb_getwidth( imp ) ) == -1 )
			{
			prnt_Scroll( "Read from <0,%d> failed.\n" );
			return	0;
			}
		if( fb_write( fbp, 0, y, scanbuf, fb_getwidth( imp ) ) == -1 )
			{
			prnt_Scroll( "Write to <0,%d> failed.\n" );
			return	0;
			}
		}
	}
	(void) fb_close( imp );
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_String( buf ) /* Place label on picture.				*/
char	*buf;
	{	static char	label[MAX_LN];
	if( ! get_Input( label, MAX_LN, "Enter text string : " ) )
		return	0;
	prnt_Event( "Drawing \"%s\".", label );
	do_line( cursor_pos.p_x, cursor_pos.p_y, label, (RGBpixel *)NULL );
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Put_Pixel( buf ) /* Put pixel.					*/
char	*buf;
	{	register int	rectwid = brush_sz / 2;
	/* If brush size is 2 or more, fill rectangle.			*/
	if( rectwid == 0 )
		{ /* Avoid overhead if only writing one pixel.		*/
#ifdef sgi
		(void) fb_write( fbp, cursor_pos.p_x, cursor_pos.p_y, paint, 1 );
#else
		(void) fb_seek( fbp, cursor_pos.p_x, cursor_pos.p_y );
		FB_WPIXEL( fbp, paint );
#endif
		}
	else
		fb_Paint( cursor_pos.p_x - rectwid, cursor_pos.p_y - rectwid,
			  cursor_pos.p_x + rectwid, cursor_pos.p_y + rectwid,
			  (RGBpixel *) paint
			  );
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Set_X_Pos( buf ) /* Move cursor's X location (image space).		*/
char	*buf;
	{	static char	x_str[5];
	if( ! get_Input( x_str, 5, "Enter X coordinate : " ) )
		return	0;
	(void) sscanf( x_str, "%d", &cursor_pos.p_x );
	if( cursor_pos.p_x > fb_getwidth(fbp) )
		cursor_pos.p_x = fb_getwidth(fbp);
	if( cursor_pos.p_x < 0 )
		cursor_pos.p_x = 0;
	reposition_cursor = true;
	return	1;
	}

_LOCAL_ int
/*ARGSUSED*/
f_Set_Y_Pos( buf ) /* Move cursor's Y location (image space).		*/
char	*buf;
	{	static char	y_str[5];
	if( ! get_Input( y_str, 5, "Enter Y coordinate : " ) )
		return	0;
	(void) sscanf( y_str, "%d", &cursor_pos.p_y );
	if( cursor_pos.p_y > fb_getheight(fbp) )
		cursor_pos.p_y = fb_getheight(fbp);
	if( cursor_pos.p_y < 0 )
		cursor_pos.p_y = 0;
	reposition_cursor = true;
	return	1;
	}

/*	p a r s _ A r g v ( )						*/
_LOCAL_ int
pars_Argv( argc, argv )
register char	**argv;
	{	register int	c;
		extern int	optind;
		extern char	*optarg;
	/* Parse options.						*/
	while( (c = getopt( argc, argv, "hp" )) != EOF )
		{
		switch( c )
			{
		case 'h' :
			cur_width = 1024;
			break;
		case 'p' :
			pad_flag = true;
			break;
		case '?' :
			return	0;
			}
		}
	if( argc != optind )
		{
		(void) fprintf( stderr, "Too many arguments!\n" );
		return	0;
		}
	return	1;
	}

/*	f b _ S e t u p ( )						*/
_LOCAL_ int
fb_Setup()
	{
	if( (fbp = fb_open( NULL, cur_width, cur_width )) == FBIO_NULL )
		{
		fb_log( "Could not open default frame buffer.\n" );
		return	-1;
		}
	fb_ioinit( fbp );
	if( fb_setcursor( fbp, cursor.bits, cursor.xbits, cursor.ybits,
	    cursor.xorig, cursor.yorig ) == -1 )
		{
		fb_log( "Can't set up cursor.\n" );
		return	0;
		}
	windo_center.p_x = cursor_pos.p_x = image_center.p_x = fb_getwidth(fbp) / 2;
	windo_center.p_y = cursor_pos.p_y = image_center.p_y = fb_getheight(fbp) / 2;
	size_viewport = fb_getwidth(fbp);
	return	0;
	}

/*	f b _ W i n d ( )						*/
_LOCAL_ void
fb_Wind()
	{
	zoom_factor = fb_getwidth(fbp) / size_viewport;
	(void) fb_zoom( fbp, zoom_factor, zoom_factor );
	windo_anchor.p_x = windo_center.p_x = cursor_pos.p_x;
	windo_anchor.p_y = windo_center.p_y = cursor_pos.p_y;
	(void) fb_window( fbp, windo_center.p_x, windo_center.p_y );
	return;
	}

/*	f b _ P a i n t ( )						*/
_LOCAL_ void
fb_Paint( x0, y0, x1, y1, color )
register int	x0, y0, x1, y1;
RGBpixel		*color;
	{	register int	x;
		Rectangle	clipped_rect;
	clipped_rect.r_origin.p_x = x0;
	clipped_rect.r_corner.p_x = x1;
	clipped_rect.r_origin.p_y = y0;
	clipped_rect.r_corner.p_y = y1;
	clip_Rectangle( &clipped_rect );
	fill_Rectangle( &clipped_rect, color ); 
	return;
	}

/*	g e n e r a l _ H a n d l e r ( )				*/
#if STD_SIGNAL_DECLS
_LOCAL_ void
#else
_LOCAL_ int
#endif
general_Handler( sig )
int	sig;
	{
	switch( sig )
		{
	case SIGHUP :
		prnt_Event( "Hangup." );
		restore_Tty();
		exit( sig );
		/*NOTREACHED*/
	case SIGINT :
		prnt_Event( "Interrupt." );
		restore_Tty();
		exit( sig );
		/*NOTREACHED*/
	case SIGQUIT :
		prnt_Event( "Quit (core dumped)." );
		restore_Tty();
		abort();
		/*NOTREACHED*/
	case SIGILL :
		prnt_Event( "Illegal instruction (core dumped)." );
		restore_Tty();
		abort();
		/*NOTREACHED*/
	case SIGIOT :
		prnt_Event( "IOT trap (core dumped)." );
		restore_Tty();
		abort();
		/*NOTREACHED*/
	case SIGBUS :
		prnt_Event( "Bus error (core dumped)." );
		restore_Tty();
		abort();
		/*NOTREACHED*/
	case SIGSEGV :
		prnt_Event( "Segmentation violation (core dumped)." );
		restore_Tty();
		abort();
		/*NOTREACHED*/
	case SIGALRM :
		break;
#ifdef SIGWINCH
	case SIGWINCH :
		break;
#endif
#ifdef SYSV
	case SIGCLD :
		break;
#else
	case SIGCHLD :
		break;
#endif

#ifdef BSD
	case SIGSTOP :
	case SIGTSTP :
		(void) f_Stop( (char *) NULL );
		break;
#else
#	ifdef VLDSYSV
	case 18 :
	case SIGUSR2 :
		(void) f_Stop( (char *) NULL );
		break;
#	endif
#endif
	default :
		prnt_Event( "\"%s\", signal(%d).", __FILE__, sig );
		restore_Tty();
		break;
		}
	(void) signal( sig, general_Handler );
#if STD_SIGNAL_DECLS
	return;
#else
	return	sig;
#endif
	}

/*	i n i t _ T t y ( )						*/
void
init_Tty()
	{
	if( pad_flag )
		{
		if( pad_open( fb_getwidth(fbp) ) == -1 )
			pad_flag = false;
		}
	save_Tty( tty_fd );
	set_Raw( tty_fd );
	clr_Tabs( tty_fd );
	clr_Echo( tty_fd );
	clr_CRNL( tty_fd );
#ifdef sgi
	sgi_Init();
#endif
	return;
	}

/*	r e s t o r e _ T t y ( )					*/
void
restore_Tty()
	{
	(void) fb_cursor( fbp, 0, 0, 0 );	/* off */
	MvCursor( 1, LI );
	if( pad_flag )
		pad_close();
	reset_Tty( tty_fd );
	return;
	}

/*	f b _ G e t _ P i x e l ( )					*/
void
fb_Get_Pixel( pixel )
RGBpixel	pixel;
	{
#ifdef sgi
	(void) fb_read( fbp, cursor_pos.p_x, cursor_pos.p_y, pixel, 1 );
#else
	(void) fb_seek( fbp, cursor_pos.p_x, cursor_pos.p_y );
	(void) fb_rpixel( fbp, pixel );
#endif
	return;
	}

/*	g e t _ F b _ P a n e l ( )					*/
RGBpixel	*
get_Fb_Panel( rectp )
register Rectangle	*rectp;
	{	register int	top;
		register int	rectwid;
		int		recthgt;
		int		btm, lft, rgt;
		RGBpixel	*panel;
		unsigned	u;
	lft = rectp->r_origin.p_x;
	rgt = rectp->r_corner.p_x;
	btm = rectp->r_origin.p_y;
	top = rectp->r_corner.p_y;
	if( lft > rgt )
		{
		lft = rgt;
		rgt = rectp->r_origin.p_x;
		}
	if( btm > top )
		{
		top = btm;
		btm = rectp->r_origin.p_y;
		}
	rectwid = rgt-lft + 1;
	recthgt = top-btm + 1;
	u = (rectwid*recthgt) * sizeof(RGBpixel);
	if( (panel = (RGBpixel *) malloc( u )) == RGBPIXEL_NULL )
		fb_log(	"\"%s\" (%d), get_Fb_Panel() : malloc %d (%d*%d) failed.\n",
			__FILE__, __LINE__,
			u, rectwid, recthgt
			);
	else
		{	register int	y = btm;
			RGBpixel	*pixelp = panel;
		for( ; y <= top; y++, pixelp += rectwid )
			{
			if( fb_read( fbp, lft, y, pixelp, rectwid ) == -1 )
				{
				fb_log( "Read of %d pixels from <%d,%d> failed.\n",
					rectwid, lft, y
					);
				return	panel;
				}
			}
		}
	return	panel;
	}

/*	p u t _ F b _ P a n e l ( )					*/
_LOCAL_ void
put_Fb_Panel( rectp, panel )
register Rectangle	*rectp;
register RGBpixel	*panel;
	{	register int	top, rectwid, y;
		int		lft, rgt, btm;
	rectwid = rectp->r_corner.p_x - rectp->r_origin.p_x + 1;
	clip_Rectangle( rectp );
	lft = rectp->r_origin.p_x;
	rgt = rectp->r_corner.p_x;
	btm = rectp->r_origin.p_y;
	top = rectp->r_corner.p_y;
	for( y = btm; y <= top; y++, panel += rectwid )
		{
		if( fb_write( fbp, lft, y, panel, rgt-lft+1 ) == -1 )
			{
			fb_log( "Write of %d pixels to <%d,%d> failed.\n",
				rectwid, lft, btm
				);
			return;
			}
		}
	return;
	}

/*	g e t _ P o i n t ( )						*/
_LOCAL_ void
get_Point( msg, pointp )
char		*msg;
register Point	*pointp;
	{	register int	tag_point = -1;
		register int	status_change;
		register int	c = NUL;
	prnt_Prompt( msg );
	for( ; tag_point != 1 && c != ' '; )
		{	register int	status_change = false;
		if( *cptr != NUL )
			{
			c = *cptr++;
			do_Key_Cmd( c, 1 );
			}
		else
			{
			if( reposition_cursor )
				{
				(void) fb_cursor(	fbp,
							1,
							cursor_pos.p_x,
							cursor_pos.p_y
							);
				status_change = true;
				reposition_cursor = false;
				}
			if( ! empty( tty_fd ) )
				{
				if( (c = get_Char()) != EOF )
					{
					do_Key_Cmd( c, 1 );
					status_change = true;
					}
				else	/* EOF detected.		*/
					{
					prnt_Scroll( "Premature EOF.\n" );
					return;
					}
				}
			if(   !	reposition_cursor
			    &&	(tag_point = get_Mouse_Pos( &cursor_pos ))
				!= -1
				)
				status_change = true;
			}
		if( status_change )
			{
			(void) fb_flush( fbp );
			if( report_status )
				prnt_Status();
			}
		}
	prnt_Event( "Point picked" );
	prnt_Prompt( "" );
	*pointp = cursor_pos;
	return;
	}

/*	g e t _ R e c t a n g l e ( )					*/
_LOCAL_ void
get_Rectangle( name, rectp )
char			*name;
register Rectangle	*rectp;
	{	char	buf[MAX_LN];
	(void) sprintf( buf, "Pick lower-left corner of %s.", name );
	get_Point( buf, &rectp->r_origin );
	(void) sprintf( buf, "Pick upper-right corner of %s.", name );
	get_Point( buf, &rectp->r_corner );
	fix_Rectangle( rectp );
	return;
	}

_LOCAL_ void
fix_Rectangle( rectp )
register Rectangle	*rectp;
	{	register int	i;
	if( rectp->r_origin.p_x > rectp->r_corner.p_x )
		{
		i = rectp->r_origin.p_x;
		rectp->r_origin.p_x = rectp->r_corner.p_x;
		rectp->r_corner.p_x = i;
		}
	if( rectp->r_origin.p_y > rectp->r_corner.p_y )
		{
		i = rectp->r_origin.p_y;
		rectp->r_origin.p_y = rectp->r_corner.p_y;
		rectp->r_corner.p_y = i;
		}
	return;
	}

_LOCAL_ void
clip_Rectangle( rectp )
register Rectangle	*rectp;
	{
	rectp->r_origin.p_x = rectp->r_origin.p_x < 0 ? 0 : rectp->r_origin.p_x;
	rectp->r_corner.p_x = rectp->r_corner.p_x >= fb_getwidth(fbp) ? fb_getwidth(fbp) - 1 : rectp->r_corner.p_x;
	rectp->r_origin.p_y = rectp->r_origin.p_y < 0 ? 0 : rectp->r_origin.p_y;
	rectp->r_corner.p_y = rectp->r_corner.p_y >= fb_getheight(fbp) ? fb_getheight(fbp) - 1 : rectp->r_corner.p_y;
	return;
	}

_LOCAL_ RGBpixel	*
pixel_Avg( p1, p2, p3, p4 )
register RGBpixel	*p1, *p2, *p3, *p4;
	{	static RGBpixel	p_avg;
	p_avg[RED] = ((int) (*p1)[RED] + (int) (*p2)[RED] + (int) (*p3)[RED] + (int) (*p4)[RED]) / 4;
	p_avg[GRN] = ((int) (*p1)[GRN] + (int) (*p2)[GRN] + (int) (*p3)[GRN] + (int) (*p4)[GRN]) / 4;
	p_avg[BLU] = ((int) (*p1)[BLU] + (int) (*p2)[BLU] + (int) (*p3)[BLU] + (int) (*p4)[BLU]) / 4;
	return	(RGBpixel *)p_avg;
	}

char	*
char_To_String( i )
int	i;
	{	static char	buf[4];
	if( i >= SP && i < DEL )
		{
		buf[0] = i;
		buf[1] = NUL;
		}
	else
	if( i >= NUL && i < SP )
		{
		buf[0] = '^';
		buf[1] = i + 64;
		buf[2] = NUL;
		}
	else
	if( i == DEL )
		return	"DL";
	else
		return	"EOF";
	return	buf;
	}

get_Mouse_Pos( pointp )
Point	*pointp;
	{	extern FBIO	*fbp;
	if( pad_flag )
		return	do_Bitpad( pointp );
#ifdef sgi
	return	sgi_Mouse_Pos( &cursor_pos );
#else
	return	-1;
#endif
	}

/*	d o _ B i t p a d ( )						*/
_LOCAL_ int
do_Bitpad( pointp )
register Point	*pointp;
	{	int	press;
	if( ! pad_flag )
		return	-1;
	if( (press = getpos( &bitpad )) != -1 )
		{		
		pointp->p_x = windo_anchor.p_x +
				(bitpad.p_x-image_center.p_x)/zoom_factor;
		pointp->p_y = windo_anchor.p_y +
				(bitpad.p_y-image_center.p_y)/zoom_factor;
		(void) fb_cursor( fbp, 1, pointp->p_x, pointp->p_y );
		if( press == 1 )
			De_Bounce_Pen();
		return	press;
		}
	return	-1;
	}

int
get_Char()
	{	int	c;
#if defined( sgi ) && 0
	return	(c = sgi_Getchar()) == EOF ? EOF : toascii( c );
#else
	return	(c = getchar()) == EOF ? EOF : toascii( c );
#endif
	}

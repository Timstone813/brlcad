/*
	rle-imagen -- convert RLE image file to imPRESS bitmap

	last edit:	86/10/05	D A Gwyn

	SCCS ID:	@(#)rle-im.c	1.1

	Borrows heavily from Gary Moss's "rle-fb" utility and
	Steve Hawley's & Geoffrey Cooper's "traceconv" program.

	The image is printed upside down to simplify the arithmetic
	(necessary due to stupid RLE file format design).
*/

#ifndef lint
static char	SCCS_ID[] = "@(#)rle-im.c	1.1";	/* string for "what" */
#endif

#include	<ctype.h>
#include	<stdio.h>

/*#include	<std.h>*/

typedef int bool;
#define true 1
#define false 0

extern char	*optarg;
extern int	optind;

extern void	exit();
extern int	getopt();

typedef unsigned char u_char;
static u_char	halftone[8][8] =	/* halftone pattern */
{
	6,   7,   8,   9,  10,  11,  12,  13,
	40,  42,  44,  46,  49,  51,  54,  14,
	37, 112, 118, 124, 130, 137,  57,  15,
	36, 106, 208, 219, 230, 145,  60,  16,
	34, 101, 197, 254, 243, 152,  63,  17,
	32,  96, 187, 178, 169, 160,  66,  18,
	30,  91,  86,  82,  78,  74,  70,  19,
	29,  27,  26,  25,  23,  22,  21,  20
};

static u_char	dither[8][8] =		/* dither pattern */
{
	6,  51,  14,  78,   8,  57,  16,  86,
	118,  22, 178,  34, 130,  25, 197,  37,
	18,  96,  10,  63,  20, 106,  12,  70,
	219,  42, 145,  27, 243,  46, 160,  30,
	9,  60,  17,  91,   7,  54,  15,  82,
	137,  26, 208,  40, 124,  23, 187,  36,
	21, 112,  13,  74,  19, 101,  11,  66,
	254,  49, 169,  32, 230,  44, 152,  29
};

static u_char	(*pattern)[8] = dither;	/* -> dither or halftone */

static char	*file_name = "-";	/* name of RLE file, for banner */
static int	rlesize = 512;		/* "screen" width: 512 or 1024 */

static long		swath[32][64];	/* assumes long has 32 bits */

static unsigned char	line[1024];	/* grey-scale RLE scan */

static int	im_mag;			/* magnification (2 or 4) */
static int	im_width;		/* image size (in Imagen dots) */
static int	im_patches;		/* # 32-bit patches needed */

static bool	get_args();
static bool	im_close();
static bool	im_dither();
static bool	im_header();
static bool	im_write();



main( argc, argv )
int		argc;
char		*argv[];
{
	register int	scan_ln;
	int		flags;

#ifdef never
	if ( !get_args( argc, argv ) )
	{
		(void)fputs( "Usage: rle-im [-b (WB)] [-h] [file]\n", stderr );
		return 1;
	}
#endif

	rlesize = 512;
/**	rlesize = 1024;		/* high-resolution */

	im_mag = rlesize == 512 ? 4 : 2;
	im_width  = rlesize * im_mag;
	im_patches = im_width / 32;

	if ( !im_header() )
		return 1;

	(void)im_write();

	if ( !im_close() )
	    return 1;

	return 0;
}


#ifdef never
static bool
get_args( argc, argv )
register char	**argv;
{
	register int	c;

	/* Parse options. */

	while ( (c = getopt( argc, argv, "b:h" )) != EOF )
	    {
		switch( c )
		    {
		case 'b':		/* user-specified background */
			bgflag = true;

			switch( toupper( (int)optarg[0] ) )
			    {
			case 'W':	/* White */
				bgpixel.red = bgpixel.green = bgpixel.blue = 255;
				break;

			case 'B':	/* Black */
				bgpixel.red = bgpixel.green = bgpixel.blue = 0;
				break;

			default:
				(void)fprintf( stderr,
				"rle-im: background `%c' unknown\n",
				(int)optarg[0]
				    );
				bgflag = false;
				break;
			}
			break;

		case 'h':		/* halftone instead of dither */
			pattern = halftone;
			break;

		default:		/* '?' */
			return false;
		}
	}

	if ( argv[optind] == NULL )
	    file_name = "-";
	else	{
		file_name = argv[optind];

		if ( freopen( argv[optind], "r", stdin ) == NULL )
		    {
			(void)fprintf( stderr,
			"rle-im: cannot open \"%s\" for reading\n",
			argv[optind]
			    );
			return false;
		}
	}

	if ( argc > ++optind )
	    {
		(void)fprintf( stderr, "rle-im: too many arguments\n" );
		return false;
	}

	return true;
}
#endif


static bool
im_header()
{

	(void)printf( "@document(language impress, prerasterization on, Name \"%s\")",
	file_name
	    );

	(void)putchar(205);		/* SET_HV_SYSTEM (whole page) */
	(void)putchar(0x54);
	(void)putchar(135);		/* SET_ABS_H (left margin) */
	(void)putchar(0);
	(void)putchar(192);
	(void)putchar(137);		/* SET_ABS_V (top margin) */
	(void)putchar(2);
	(void)putchar(32);
	(void)putchar(205);		/* SET_HV_SYSTEM (set origin) */
	(void)putchar(0x74);
	(void)putchar(206);		/* SET_ADV_DIRS (normal raster scan) */
	(void)putchar(0);
	(void)putchar(236);		/* SET_MAGNIFICATION */
	(void)putchar(0);		/* x 1 */
	(void)putchar(235);		/* BITMAP */
	(void)putchar(3);		/* opaque (store) */
	(void)putchar(im_patches);	/* hsize (# patches across) */
	(void)putchar(im_patches);	/* vsize (# patches down) */

	return true;
}

static bool
im_write()
{
	int y;
	int y1;
	register int x, x1;
	register int mx, my;

	for( y = 0; y < rlesize; y += 32/im_mag )  {
fprintf(stderr,"y=%d\n", y);
		if( feof(stdin) )
			return 1;
		/* Process one 32-bit high output swath */
		for ( y1 = 0; y1 < 32; y1 += im_mag )
		{
			/* Obtain a single line of 8-bit pixels */
			if( fread( line, 1, rlesize, stdin ) != rlesize )
				bzero( line, rlesize );

			/* construct im_mag scans of Imagen swath */
			for ( x = 0; x < im_width; x += 32 )
				for ( my = 0; my < im_mag; ++my )
				{
					register long	b = 0L;	/* image bits */

					for ( x1 = 0; x1 < 32; x1 += im_mag )
					{
						register int	level =
						    line[rlesize-1-((x + x1) / im_mag)];

						for ( mx = 0; mx < im_mag; ++mx )
						{
							b <<= 1;

							if ( im_dither(	x + x1 + mx,
							y + y1 + my,
							level
							    )
								)
								    b |= 1L;
						}
					}

					swath[y1 + my][x / 32] = b;
				}
		}

		/* output the swath */
		for ( x1 = 0; x1 < im_patches; ++x1 )
			for ( y1 = 0; y1 < 32; ++y1 )
			{
				register long	b = swath[y1][x1];

				(void)putchar( (int)(b >> 24) & 0xFF );
				(void)putchar( (int)(b >> 16) & 0xFF );
				(void)putchar( (int)(b >> 8) & 0xFF );
				(void)putchar( (int)b & 0xFF );
			}
	}

	return true;
}


static bool
im_dither( x, y, level )
register int	x, y;		/* coords of page point position */
int		level;		/* grey level (0..255) */
{
	if ( pattern == halftone && y % 16 >= 8 )
	    x += 4;			/* ameliorate grid regularity */

	return (u_char)level < pattern[x % 8][y % 8];
}


static bool
im_close()
{
	/*	(void)putchar( 219 );		/* ENDPAGE */
	/*	(void)flush( stdout );	*/

	return true;
}

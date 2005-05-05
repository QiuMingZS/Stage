/*************************************************************************
 * RTV
 * $Id: matrix.c,v 1.16 2005-05-05 20:10:30 rtv Exp $
 ************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h> // for memcpy(3)

#include "stage_internal.h"

//#define DEBUG

// if this is defined, empty matrix cells are deleted, saving memory
// at the cost of a little time. If you turn this off - Stage may use
// a LOT of memory if it runs a long time in large worlds.
#define STG_DELETE_EMPTY_CELLS 1

// basic integer drawing functions (not intended for external use)
void draw_line( stg_matrix_t* matrix,
		int x1, int y1, int x2, int y2, void* object, int add);

void matrix_key_destroy( gpointer key )
{
  free( key );
}

void matrix_value_destroy( gpointer value )
{
  g_ptr_array_free( (GPtrArray*)value, TRUE );
}

// compress the two (long) coordinates into a single (ulong) value
guint matrix_hash( gconstpointer key )
{
  stg_matrix_coord_t* coord = (stg_matrix_coord_t*)key;
  
  long x = coord->x * 73856093; // big prime numbers
  long y = coord->y * 19349663;  
  long z = x ^ y; // XOR
  
  return( (guint)z );
  
  //gshort x = ; // ignore overflow
  //gshort y = (gshort)coord->y; // ignore overflow
  
  //printf( " %d %d hash %d\n", x, y, ( (x << 16) | y ) );

  //return( (guint)((((gshort)coord->x) << 16) | (gshort)coord->y) );
}

// returns TRUE if the two coordinates are the same, else FALSE
gboolean matrix_equal( gconstpointer c1, gconstpointer c2 )
{
  //stg_matrix_coord_t* mc1 = (stg_matrix_coord_t*)c1;
  //stg_matrix_coord_t* mc2 = (stg_matrix_coord_t*)c2;
  
  return( ((stg_matrix_coord_t*)c1)->x == ((stg_matrix_coord_t*)c2)->x && 
	  ((stg_matrix_coord_t*)c1)->y == ((stg_matrix_coord_t*)c2)->y );  
}

stg_matrix_t* stg_matrix_create( double ppm, double medppm, double bigppm )
{
  stg_matrix_t* matrix = (stg_matrix_t*)calloc(1,sizeof(stg_matrix_t));
  
  matrix->table = g_hash_table_new_full( matrix_hash, matrix_equal,
					 matrix_key_destroy, matrix_value_destroy );  

  matrix->medtable = g_hash_table_new_full( matrix_hash, matrix_equal,
					    matrix_key_destroy, matrix_value_destroy );  
  
  matrix->bigtable = g_hash_table_new_full( matrix_hash, matrix_equal,
					    matrix_key_destroy, matrix_value_destroy );
  
  matrix->ppm = ppm;
  matrix->medppm = medppm;  
  matrix->bigppm = bigppm;

  return matrix;
}

// frees all memory allocated by the matrix; first the cells, then the
// cell array.
void stg_matrix_destroy( stg_matrix_t* matrix )
{
  g_hash_table_destroy( matrix->table );
  g_hash_table_destroy( matrix->medtable );
  g_hash_table_destroy( matrix->bigtable );
  free( matrix );
}

// removes all pointers from every cell in the matrix
void stg_matrix_clear( stg_matrix_t* matrix )
{  
  g_hash_table_destroy( matrix->table );
  matrix->table = g_hash_table_new_full( matrix_hash, matrix_equal,
					 matrix_key_destroy, matrix_value_destroy );
}

GPtrArray* stg_table_cell( GHashTable* table, glong x, glong y)
{
  stg_matrix_coord_t coord;
  coord.x = x;
  coord.y = y;
 
  //printf( "table %p fetching [%ld][%ld]\n", table, x, y );
  return g_hash_table_lookup( table, &coord );
}

GPtrArray* stg_matrix_table_add_cell( GHashTable* table, glong x, glong y )
{
  stg_matrix_coord_t* coord = calloc( sizeof(stg_matrix_coord_t), 1 );
  coord->x = x;
  coord->y = y;
  
  GPtrArray* cell = g_ptr_array_new();
  g_hash_table_insert( table, coord, cell );
  
  return cell;
}



// get the array of pointers in cell y*width+x, specified in meters
GPtrArray* stg_matrix_cell_get( stg_matrix_t* matrix, double x, double y)
{
  //printf( "fetching [%ld][%ld]\n", 
  //  (glong)floor(x * matrix->ppm), 
  //  (glong)floor(y * matrix->ppm) );
  
  return stg_table_cell( matrix->table, 
			 (glong)(x * matrix->ppm), 
			 (glong)(y * matrix->ppm) );  
}

// get the array of pointers in cell y*width+x, specified in meters
GPtrArray* stg_matrix_bigcell_get( stg_matrix_t* matrix, double x, double y)
{
  return stg_table_cell( matrix->bigtable, 
			 (glong)(x * matrix->bigppm), 
			 (glong)(y * matrix->bigppm) );  
}

// get the array of pointers in cell y*width+x, specified in meters
GPtrArray* stg_matrix_medcell_get( stg_matrix_t* matrix, double x, double y)
{
  return stg_table_cell( matrix->medtable, 
			 (glong)(x * matrix->medppm), 
			 (glong)(y * matrix->medppm) );  
}


// append the pointer <object> to the pointer array at the cell
void stg_matrix_cell_append(  stg_matrix_t* matrix, 
			      double x, double y, void* object )
{
  GPtrArray* cell = stg_matrix_cell_get( matrix, x, y );
 
  // if the cell is empty we create a new ptr array for it
  if( cell == NULL )
    cell = stg_matrix_table_add_cell( matrix->table, 
				      (glong)(x*matrix->ppm),
				      (glong)(y*matrix->ppm) );
  else // make sure we're only in the cell once 
    g_ptr_array_remove_fast( cell, object );
  
  g_ptr_array_add( cell, object );  
  

  GPtrArray* medcell = stg_matrix_medcell_get( matrix, x, y );
  
  // if the cell is empty we create a new ptr array for it
  if( medcell == NULL )
    medcell = stg_matrix_table_add_cell( matrix->medtable, 
					 (glong)(x*matrix->medppm),
					 (glong)(y*matrix->medppm) );
  else // make sure we're only in the cell once 
    g_ptr_array_remove_fast( medcell, object );
  
  g_ptr_array_add( medcell, object );  
  

  GPtrArray* bigcell = stg_matrix_bigcell_get( matrix, x, y );
  
  if( bigcell == NULL )
    bigcell = stg_matrix_table_add_cell( matrix->bigtable, 
					 (glong)(x*matrix->bigppm), 
					 (glong)(y*matrix->bigppm) );
  else // make sure we're only in the cell once 
    g_ptr_array_remove_fast( bigcell, object );
  
  g_ptr_array_add( bigcell, object );  

  // todo - record a timestamp for matrix mods so devices can see if
  //the matrix has changed since they last peeked into it
  // matrix->last_mod =

  //printf( "appending %p to matrix at %d %d. %d pointers now cell\n", 
  //  object, (int)x, (int)y, cell->len );
}

// if <object> appears in the cell's array, remove it
void stg_matrix_cell_remove( stg_matrix_t* matrix, 
			     double x, double y, void* object )
{ 
  GPtrArray* cell = stg_matrix_cell_get( matrix, x, y );
  
  if( cell )
    {
      //printf( "removing %p from %d,%d. %d pointers remain here\n", 
      //      object, x, y, cell->len );
      
      g_ptr_array_remove_fast( cell, object );

#if STG_DELETE_EMPTY_CELLS
      // if the ptr array is empty, we delete it to save memory.
      if( cell->len == 0 )
	{
	  stg_matrix_coord_t coord;
	  coord.x = (glong)(x*matrix->ppm);
	  coord.y = (glong)(y*matrix->ppm);
	  
	  g_hash_table_remove( matrix->table, &coord );
	  //GPtrArray* p = stg_matrix_cell( matrix, x, y );
	  //printf( "removed cell %d %d now %p\n", x, y, p  );
	}
#endif
    }
  
  // don't bother deleting med cells, there aren't nearly so many of 'em.
  GPtrArray* medcell = stg_matrix_medcell_get( matrix, x, y );
  if( medcell )
    g_ptr_array_remove_fast( medcell, object );

  // don't bother deleting big cells, there aren't nearly so many of 'em.
  GPtrArray* bigcell = stg_matrix_bigcell_get( matrix, x, y );
  if( bigcell )
    g_ptr_array_remove_fast( bigcell, object );
}

// draw or erase a straight line between x1,y1 and x2,y2.
void stg_matrix_line( stg_matrix_t* matrix,
		      double x1,double y1,double x2,double y2, 
		      void* object, int add )
{
  double xdiff = x2 - x1;
  double ydiff = y2 - y1;

  double angle = atan2( ydiff, xdiff );
  double len = hypot( ydiff, xdiff );

  double cosa = cos(angle);
  double sina = sin(angle);
  
  double xincr = cosa * 1.0 / matrix->ppm;
  double yincr = sina * 1.0 / matrix->ppm;

  // hack - try to avoid the gappy lines
  xincr *= 0.99;
  yincr *= 0.99;

  double xx = 0;
  double yy = 0;
  
  double finalx, finaly;

  while( len > hypot( yy,xx ) )
    {
      finalx = x1+xx;
      finaly = y1+yy;
      
      // bounds check
      //if( finalx >= 0 && finaly >= 0 )
	{
	  if( add ) 
	    stg_matrix_cell_append( matrix, finalx, finaly, object ); 
	  else
	    stg_matrix_cell_remove( matrix, finalx, finaly, object ); 
	}
      
      xx += xincr;
      yy += yincr;


      //printf( "line cell: %.3f %.3f\n", finalx, finaly );
    }
}

void stg_matrix_lines( stg_matrix_t* matrix, 
		       stg_line_t* lines, int num_lines,
		       void* object, int add )
{
  int l;
  for( l=0; l<num_lines; l++ )
    stg_matrix_line( matrix, 
		     lines[l].x1, lines[l].y1, 
		     lines[l].x2, lines[l].y2, 
		     object, add );
}

// render a polygon into [matrix] with origin [x,y,a]
void stg_matrix_polygon( stg_matrix_t* matrix,
			 double x, double y, double a,
			 stg_polygon_t* poly,
			 void* object, int add )
{
  // need at leats three points for a meaningful polygon
  if( poly->points->len > 2 )
    {
      int count = poly->points->len;
      int p;
      for( p=0; p<count; p++ ) // for
	{
	  stg_point_t* pt1 = &g_array_index( poly->points, stg_point_t, p );	  
	  stg_point_t* pt2 = &g_array_index( poly->points, stg_point_t, (p+1) % count);
	  
	  double gx1 = x + pt1->x * cos(a) - pt1->y * sin(a);
	  double gy1 = y + pt1->x * sin(a) + pt1->y * cos(a); 
	  
	  double gx2 = x + pt2->x * cos(a) - pt2->y * sin(a);
	  double gy2 = y + pt2->x * sin(a) + pt2->y * cos(a); 
	  
	  stg_matrix_line( matrix, 
			   gx1, gy1, gx2, gy2,
			   object, add );
	}
    }
  else
    PRINT_WARN( "attempted to matrix render a polygon with less than 3 points" ); 
}     

// render an array of [num_polys] polygons
void stg_matrix_polygons( stg_matrix_t* matrix,
			  double x, double y, double a,
			  stg_polygon_t* polys, int num_polys,
			  void* object, int add )
{
  //printf( "rendering model %s at %.2f %.2f %d\n",
  //  ((stg_model_t*)object)->token, x, y, add );
  
  int p;
  for( p=0; p<num_polys; p++ )
    stg_matrix_polygon( matrix, x,y,a, &polys[p], object, add );
}

void stg_matrix_rectangle( stg_matrix_t* matrix, 
			   double px, double py, double pth,
			   double dx, double dy, 
			   void* object, int add )
{
  dx /= 2.0;
  dy /= 2.0;

  double cx = dx * cos(pth);
  double cy = dy * cos(pth);
  double sx = dx * sin(pth);
  double sy = dy * sin(pth);
    
  double toplx =  px + cx - sy;
  double toply =  py + sx + cy;

  double toprx =  px + cx + sy;
  double topry =  py + sx - cy;

  double botlx =  px - cx - sy;
  double botly =  py - sx + cy;

  double botrx =  px - cx + sy;
  double botry =  py - sx - cy;
    
  stg_matrix_line( matrix, toplx, toply, toprx, topry, object, add);
  stg_matrix_line( matrix, toprx, topry, botrx, botry, object, add);
  stg_matrix_line( matrix, botrx, botry, botlx, botly, object, add);
  stg_matrix_line( matrix, botlx, botly, toplx, toply, object, add);

  //printf( "SetRectangle drawing %d,%d %d,%d %d,%d %d,%d\n",
  //  toplx, toply,
  //  toprx, topry,
  //  botlx, botly,
  //  botrx, botry );
}



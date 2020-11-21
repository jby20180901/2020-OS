#ifndef __THREAD_FIXED_POINT_H
#define __THREAD_FIXED_POINT_H
/* typedef int fp_t; */
typedef int fp_t;
/* fp_base 16 */
#define fp_base 16
/* Convert n to fixed point:	n * f */
#define int_to_fp(a) ((fp_t) ( a << fp_base )) 
/* Convert x to integer (rounding toward zero):	x / f */
#define fp_to_int(a) ((int) ( a >> fp_base ))
/* Convert x to integer (rounding to nearest):	(x + f / 2) / f if x >= 0, (x - f / 2) / f if x <= 0. */
#define fp_to_int_round(a) ((a >= 0) ? ( (a + ( 1 << ( fp_base - 1 ) ) ) >> fp_base) : ( (a - ( 1 << ( fp_base - 1 ) ) ) >> fp_base) )
/* Add x and y:	x + y */
#define fp_add_fp(a,b) ( a + b )
/* Subtract y from x:	x - y */
#define fp_sub_fp(a,b) ( a - b )
/* Add x and n:	x + n * f */
#define fp_add_int(a,b) ( a + ( b << fp_base ) )
/* Subtract n from x:	x - n * f */
#define fp_sub_int(a,b) ( a - ( b << fp_base ) )
/* Multiply x by y:	((int64_t) x) * y / f */
#define fp_mul_fp(a,b) (( fp_t )( ((int64_t)a) * b >> fp_base ))
/* Multiply x by n:	x * n */
#define fp_mul_int(a,b) ( a * b )
/* Divide x by y:	((int64_t) x) * f / y */
#define fp_div_fp(a,b) (( fp_t )(( ((int64_t)a) << fp_base) / b ))
/* Divide x by n:	x / n */
#define fp_div_int(a,b) ( a / b )
#endif /* thread/fixed_point.h */
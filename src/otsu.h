/*

    see README for EMAIL-address

 */


/*======================================================================*/
/* OTSU global thresholding routine                                     */
/*   takes a 2D unsigned char array pointer, number of rows, and        */
/*   number of cols in the array. returns the value of the threshold    */
/*======================================================================*/
/* JS 2019-04 changed 2nd+3th argument, new: 2nd=nx=cols, 3th=ny=rows */
int
otsu (unsigned char *image, int cols, int rows,
      int x0, int y0, int dx, int dy, int vvv);


/*======================================================================*/
/* thresholding the image  (set threshold to 128+32=160=0xA0)           */
/*   now we have a fixed thresholdValue good to recognize on gray image */
/*   - so lower bits can used for other things (bad design?)            */
/*======================================================================*/
/* JS 2019-04 changed 2nd+3th argument, new: 2nd=nx=cols, 3th=ny=rows */
int
thresholding (unsigned char *image, int cols, int rows,
              int x0, int y0, int dx, int dy, int thresholdValue);

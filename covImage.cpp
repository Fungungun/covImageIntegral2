/*
* Chenghuan Liu, Du Huynh, Jan 2017.
*/
#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <math.h>
#include <opencv2/opencv.hpp>
#include <time.h>

#include <sstream>
#include <string>

#include <omp.h>
#include <assert.h>

#include "covImage.h"
#include "debug.h"


/* ------------------------------------------------------------ */
void CovImage::SetSearchArea(Mat &tarpos){
    //SearchArea: x1,y1,x2,y2
    //(x1,y1) is the top left corner 
    //(x1,y2) is the bottom right corner
    mSearchArea.resize(4);
    double *ptarpos     = tarpos.ptr<double>(0);
    double TargetWidth  = *(ptarpos+2) - *(ptarpos);
    double TargetHeight = *(ptarpos+3) - *(ptarpos+1); 
    //cout<<TargetHeight<<" "<<TargetWidth<<endl;
    double SearchAreaMargin = TargetHeight > TargetWidth ? 
                              TargetWidth : TargetHeight;

    mSearchArea[0]          = *ptarpos - SearchAreaMargin <= 0 ? 
        0 : (int)(*ptarpos - SearchAreaMargin);
    mSearchArea[1]         = *(ptarpos+1) - SearchAreaMargin <= 0 ?
        0 : (int)(*(ptarpos+1) - SearchAreaMargin);
    mSearchArea[2]     = *(ptarpos+2) + SearchAreaMargin >= nCols ?
nCols : (int)(*(ptarpos+2) + SearchAreaMargin);
    mSearchArea[3]     = *(ptarpos+3) + SearchAreaMargin >= nRows ?
nRows : (int)(*(ptarpos+3) + SearchAreaMargin);
}

/* ------------------------------------------------------------ */
void CovImage::imin_rgb2lab(){
    if (im_in.channels() == 3){
        //following these steps to convert RGB to 64FLab
        //rgb -> CV_32F -> Lab -> CV_64F
        Mat tmp;
        im_in.convertTo(tmp,CV_32F);
        tmp *= 1./255;
        cvtColor(tmp,im,CV_BGR2Lab);
        im.convertTo(im,CV_64F);
    }
    else{
        im_in.convertTo(im,CV_64F);
    }
    nChannels = im.channels();
    nRows = im.rows;
    nCols = im.cols;
    dim = nChannels*5 + 2;
    /*
    cerr << "nRows = " << nRows << " Ncols = " << nCols;
    cerr << " dim = " << dim << "\n";
    */
}

/* ------------------------------------------------------------ */
void CovImage::process()
{
    assert(dim == FEAT_DIM1 || dim == FEAT_DIM3);

    // initialize featimage
    if (dim == FEAT_DIM1)
    {
        featimage = Mat_<Vec<double,FEAT_DIM1> >(nRows, nCols);
    }
    else
    {
        featimage = Mat_<Vec<double,FEAT_DIM3> >(nRows, nCols);
    }

    coordinateX();
    coordinateY();

    //  		cerr << "coordinateX() done\n";
    //  		debug::printDoubleMat(featimage, 0);
    //  		cerr << "coordinateY() done\n";
    //  		debug::printDoubleMat(featimage, 1);


    for (int c=0; c < nChannels; c++) {
        intensity(c);
        gradientX(c);
        gradientY(c);  // should be in channel 2+2*nChannels+c
        gradient2X(c); // channel 2+3*nChannels+c
        gradient2Y(c); // channel 2+4*nChannels+c

        //          cerr << "intensity() channel " << c << " done\n";
        //          debug::printDoubleMat(featimage, 2+c);
        //          cerr << "gradientX() channel " << c << " done\n";
        //          debug::printDoubleMat(featimage, 2+nChannels+c);
        // 			cerr << "gradientY() channel " << c << " done\n";
        // 			debug::printDoubleMat(featimage, 2+2*nChannels+c);
        // 			cerr << "gradient2X() channel " << c << " done\n";
        // 			debug::printDoubleMat(featimage, 2+3*nChannels+c);
        // 			cerr << "gradient2Y() channel " << c << " done\n";
        // 			debug::printDoubleMat(featimage, 2+4*nChannels+c);	
    }
    computeIntegralImage();
}


/* ------------------------------------------------------------ */
void CovImage::coordinateX()
{
    double *outptr;
    /*
    cerr << "coordX, #channels = " << featimage.channels() << " \n";
    cerr << "(nRows,nCols) = ( " << featimage.rows << "," << featimage.cols << " )\n";
    cerr << "depth =  " << featimage.depth() << ", CV_64F = " << CV_64F << "\n";
    */
    for (int r=0; r < nRows; r++) 
    {
        outptr = (double *)featimage.ptr<double>(r);
        for (int c=0; c < nCols; c++, outptr += dim)
        {
            *outptr = (double)c;
        }
    }
}
/* ------------------------------------------------------------ */

void CovImage::coordinateY()
{
    double *outptr;
    for (int r=0; r < nRows; r++)
    {
        outptr = (double *)featimage.ptr<double>(r) + 1;
        for (int c=0; c < nCols; c++, outptr += dim)
        {
            *outptr = (double)r;
        }
    }
}
/* ------------------------------------------------------------ */

void CovImage::intensity(int channel)
{
    // uchar *inptr;
    double *inptr;
    double *outptr;
    for(int r=0; r < nRows; r++)
    {
        // inptr = (uchar *)im.ptr<uchar>(r) + channel;
        inptr = (double *)im.ptr<double>(r) + channel;
        outptr = (double *)featimage.ptr<double>(r) + channel + 2;
        for (int c=0; c < nCols; c++, inptr += nChannels, outptr += dim)
        {
            *outptr = (double)(*inptr);
        }
    }
}
/* ------------------------------------------------------------ */
/** Make sure that the 'intensity' function is called before this function. */
void CovImage::gradientX(int channel)
{
    double *inptr;  // pointing to intensity of the channel
    double *outptr; // pointing to gradient-x of the channel
    int offset = channel + 2;
    for (int r=0; r < nRows; r++) 
    {
        inptr = (double *)featimage.ptr<double>(r) + offset;
        outptr = (double *)featimage.ptr<double>(r) + nChannels + offset;
        // set the Ix value at column 0 the same as column 1
        *outptr = (*(inptr+2*dim) - *inptr) / 2.0;
        inptr += dim; outptr += dim;
        for (int c=1; c < nCols-1; c++, inptr += dim, outptr += dim)
        {
            *outptr = (*(inptr+dim) - *(inptr-dim)) / 2.0;
        }
        // the Ix value at the last column is the same as the previous column
        *outptr = *(outptr-dim);
    }
}
/* ------------------------------------------------------------ */

void CovImage::gradientY(int channel)
{
    double *inptr1;   // pointing to intensity of the channel
    double *inptr2;
    double *outptr;  // pointing to gradient-y of the channel

    int offset = channel + 2;
    for (int r = 1; r < nRows-1; r++)
    {
        inptr1 = (double *)featimage.ptr<double>(r-1) + offset;
        inptr2 = (double *)featimage.ptr<double>(r+1) + offset;
        outptr = (double *)featimage.ptr<double>(r) + offset + nChannels*2;
        for (int c=0; c < nCols; c++, inptr1 += dim, inptr2 += dim, outptr += dim)
        {
            *outptr = (*inptr2 - *inptr1) / 2.0;
        }
    }
    // set row 0 the same as row 1
    outptr = (double *)featimage.ptr<double>(1) + offset + nChannels*2;
    double *outptr2 = (double *)featimage.ptr<double>(0) + offset + nChannels*2;
    for (int c=0; c < nCols; c++, outptr += dim, outptr2 += dim)
    {
        *outptr2 = *outptr;
    }

    // set the last row the same as the second last row
    outptr = (double *)featimage.ptr<double>(nRows-2) + offset + nChannels*2;
    outptr2 = (double *)featimage.ptr<double>(nRows-1) + offset + nChannels*2;
    for (int c=0; c < nCols; c++, outptr += dim, outptr2 += dim)
    {
        *outptr2 = *outptr;
    }
}
/* ------------------------------------------------------------ */

void CovImage::gradient2X(int channel)
{
    double *inptr;  // pointing to intensity of the channel
    double *outptr; // pointing to gradient-x of the channel
    int offset = channel + 2;
    for(int r=0; r < nRows; r++)
    {
        inptr = (double *)featimage.ptr<double>(r) + offset;
        outptr = (double *)featimage.ptr<double>(r) + nChannels*3 + offset;
        // set the Ix value at column 0 the same as column 1
        *outptr = (*(inptr+2*dim) - 2*(*(inptr+dim)) + *inptr) / 1.0;
        inptr += dim; outptr += dim;
        for (int c=1; c < nCols-1; c++, inptr += dim, outptr += dim)
        {
            *outptr = (*(inptr+dim) - 2 * (*inptr) + *(inptr-dim)) / 1.0;
        }
        // the Ix value at the last column is the same as the previous column
        *outptr = *(outptr-dim);
    }
}
/* ------------------------------------------------------------ */

void CovImage::gradient2Y(int channel){
    double *inptr1;   // pointing to intensity of the channel
    double *inptr2;
    double *inptr3;
    double *outptr;  // pointing to gradient-y of the channel

    int offset = channel + 2;
    for (int r = 1; r < nRows-1; r++)
    {
        inptr1 = (double *)featimage.ptr<double>(r-1) + offset;
        inptr2 = (double *)featimage.ptr<double>(r+1) + offset;
        inptr3 = (double *)featimage.ptr<double>(r) + offset;
        outptr = (double *)featimage.ptr<double>(r) + offset + nChannels*4;
        for (int c=0; c < nCols; c++, inptr1 += dim, inptr2 += dim, inptr3 += dim, outptr += dim)
        {
            *outptr = *inptr2 + *inptr1 - 2*(*inptr3);
        }
    }
    // set row 0 the same as row 1
    outptr = (double *)featimage.ptr<double>(1) + offset + nChannels*4;
    double *outptr2 = (double *)featimage.ptr<double>(0) + offset + nChannels*4;
    for (int c=0; c < nCols; c++, outptr += dim, outptr2 += dim)
    {
        *outptr2 = *outptr;
    }

    // set the last row the same as the second last row
    outptr = (double *)featimage.ptr<double>(nRows-2) + offset + nChannels*4;
    outptr2 = (double *)featimage.ptr<double>(nRows-1) + offset + nChannels*4;
    for (int c=0; c < nCols; c++, outptr += dim, outptr2 += dim)
    {
        *outptr2 = *outptr;
    }
}
/* ------------------------------------------------------------ */

void CovImage::computeIntegralImage()
{
    // initialize and compute the integral image
    int L = total(dim);
    assert(L == II_DIM1 || L == II_DIM3);
    if (L == II_DIM1) 
    {
        IIprod = Mat_<Vec<double,II_DIM1> >(nRows+1, nCols+1);
        IIsum = Mat_<Vec<double,FEAT_DIM1> >(nRows+1, nCols+1);
    }
    else 
    {
        IIprod = Mat_<Vec<double,II_DIM3> >(nRows+1, nCols+1);
        IIsum = Mat_<Vec<double,FEAT_DIM3> >(nRows+1, nCols+1);
    }

    IIprod.setTo(0.0);
    //for (int r=1; r < nRows+1; r++) 
    for(int r = mSearchArea[1]+1; r < mSearchArea[3]+1; r++) 
    {
        double *featptr, *prodptr;
        vector<double *> ptr(dim);
        //featptr = (double *)featimage.ptr<double>(r-1);
        //prodptr = (double *)IIprod.ptr<double>(r,1);
        //for (int c=1; c < nCols+1; c++, featptr += dim)
        featptr = (double *)featimage.ptr<double>(r-1,mSearchArea[0]);
        prodptr = (double *)IIprod.ptr<double>(r,mSearchArea[0]+1);
        for(int c=mSearchArea[0]+1; c < mSearchArea[2]+1; c++, featptr += dim) 
        {
            // construct IIprod
            for(int d=0; d < dim; d++)
            {
                ptr[d] = featptr + d;
            }
            for(int l=0, d1=0, d2=0; l < L; l++, prodptr++) 
            {
                *prodptr = (*ptr[d1]) * (*ptr[d2]);
                if (++d2 >= dim) 
                {
                    ++d1; d2 = d1;
                }
            }
        }
    }

    /* IIsum is a copy of featimage with an extra row of 0 at the top and extra
    * columns of 0 at the left. The #extra columns = dim
    */

    IIsum.setTo(0.0);
    //for (int r=1; r < nRows+1; r++) 
    for(int r=mSearchArea[1]+1; r < mSearchArea[3]+1; r++) 
    {
        double *ptr = (double *)IIsum.ptr<double>(r) + dim;
        double *inptr = (double *)featimage.ptr<double>(r-1);
        memcpy(ptr, inptr, sizeof(double)*dim*nCols);
    }

    /* now compute the cumulative sum along the rows then along the columns
    */
    //for (int r=1; r < nRows+1; r++) 
    for(int r=mSearchArea[1]+1; r < mSearchArea[3]+1; r++) 
    {
        IIprod.row(r) += IIprod.row(r-1);
        IIsum.row(r) += IIsum.row(r-1);
    }
    //for (int c=1; c < nCols+1; c++) 
    for(int c=mSearchArea[0]+1; c < mSearchArea[2]+1; c++)
    {
        IIprod.col(c) += IIprod.col(c-1);
        IIsum.col(c) += IIsum.col(c-1);
    }

}
/* ------------------------------------------------------------ */

Vec<double,II_DIM1> CovImage::interpIIprod1(double x, double y){
    int x0 = (int)(floor(x));
    int x1 = (int)(ceil(x));
    int y0 = (int)(floor(y));
    int y1 = (int)(ceil(y));
    double s = x-x0, t = y-y0;

    Vec<double,II_DIM1> pv0 = IIprod.at<Vec<double,II_DIM1> >(y0, x0);
    Vec<double,II_DIM1> pv1 = IIprod.at<Vec<double,II_DIM1> >(y0, x1);
    Vec<double,II_DIM1> pv2 = IIprod.at<Vec<double,II_DIM1> >(y1, x0);
    Vec<double,II_DIM1> pv3 = IIprod.at<Vec<double,II_DIM1> >(y1, x1);
    Vec<double,II_DIM1> vprod = (1-s)*(1-t)*pv0 + s*(1-t)*pv1 +
        (1-s)*t*pv2 + s*t*pv3;
    return vprod;
}
/* ------------------------------------------------------------ */

Vec<double,II_DIM3> CovImage::interpIIprod3(double x, double y){
    int x0 = (int)(floor(x));
    int x1 = (int)(ceil(x));
    int y0 = (int)(floor(y));
    int y1 = (int)(ceil(y));
    double s = x-x0, t = y-y0;

    Vec<double,II_DIM3> pv0 = IIprod.at<Vec<double,II_DIM3> >(y0, x0);
    Vec<double,II_DIM3> pv1 = IIprod.at<Vec<double,II_DIM3> >(y0, x1);
    Vec<double,II_DIM3> pv2 = IIprod.at<Vec<double,II_DIM3> >(y1, x0);
    Vec<double,II_DIM3> pv3 = IIprod.at<Vec<double,II_DIM3> >(y1, x1);
    Vec<double,II_DIM3> vprod = (1-s)*(1-t)*pv0 + s*(1-t)*pv1 +
        (1-s)*t*pv2 + s*t*pv3;
    return vprod;
}
/* ------------------------------------------------------------ */

Vec<double,FEAT_DIM1> CovImage::interpIIsum1(double x, double y){
    int x0 = (int)(floor(x));
    int x1 = (int)(ceil(x));
    int y0 = (int)(floor(y));
    int y1 = (int)(ceil(y));
    double s = x-x0, t = y-y0;

    Vec<double,FEAT_DIM1> sv0 = IIsum.at<Vec<double,FEAT_DIM1> >(y0, x0);
    Vec<double,FEAT_DIM1> sv1 = IIsum.at<Vec<double,FEAT_DIM1> >(y0, x1);
    Vec<double,FEAT_DIM1> sv2 = IIsum.at<Vec<double,FEAT_DIM1> >(y1, x0);
    Vec<double,FEAT_DIM1> sv3 = IIsum.at<Vec<double,FEAT_DIM1> >(y1, x1);
    Vec<double,FEAT_DIM1> vsum = (1-s)*(1-t)*sv0 + s*(1-t)*sv1 +
        (1-s)*t*sv2 + s*t*sv3;
    return vsum;
}
/* ------------------------------------------------------------ */

Vec<double,FEAT_DIM3> CovImage::interpIIsum3(double x, double y){
    int x0 = (int)(floor(x));
    int x1 = (int)(ceil(x));
    int y0 = (int)(floor(y));
    int y1 = (int)(ceil(y));
    double s = x-x0, t = y-y0;

    Vec<double,FEAT_DIM3> sv0 = IIsum.at<Vec<double,FEAT_DIM3> >(y0, x0);
    Vec<double,FEAT_DIM3> sv1 = IIsum.at<Vec<double,FEAT_DIM3> >(y0, x1);
    Vec<double,FEAT_DIM3> sv2 = IIsum.at<Vec<double,FEAT_DIM3> >(y1, x0);
    Vec<double,FEAT_DIM3> sv3 = IIsum.at<Vec<double,FEAT_DIM3> >(y1, x1);
    Vec<double,FEAT_DIM3> vsum = (1-s)*(1-t)*sv0 + s*(1-t)*sv1 + (1-s)*t*sv2
        + s*t*sv3;
    return vsum;
}
/* ------------------------------------------------------------ */

void CovImage::covComponentMatrices(double x1, double y1, double x2, double y2, Mat &prodM, Mat &sumM, double &Npixels)
{
    int L = IIprod.channels();

    assert(x2 > x1 && y2 > y1 && x1 >= 0 && y1 >= 0 &&
        x2 < nCols && y2 < nRows);

    Npixels = (x2-x1+1) *(y2-y1+1);

    prodM = Mat_<double>(dim,dim,CV_64F);
    if (L == II_DIM1)
    {
        Vec<double,II_DIM1> pv1 = interpIIprod1(x1, y1);
        Vec<double,II_DIM1> pv2 = interpIIprod1(x2+1.0, y1);
        Vec<double,II_DIM1> pv3 = interpIIprod1(x1, y2+1.0);
        Vec<double,II_DIM1> pv4 = interpIIprod1(x2+1.0, y2+1.0);
        Vec<double,II_DIM1> pv = pv4 + pv1 - pv2 - pv3;
        Vec<double,FEAT_DIM1> sv1 = interpIIsum1(x1, y1);
        Vec<double,FEAT_DIM1> sv2 = interpIIsum1(x2+1.0, y1);
        Vec<double,FEAT_DIM1> sv3 = interpIIsum1(x1, y2+1.0);
        Vec<double,FEAT_DIM1> sv4 = interpIIsum1(x2+1.0, y2+1.0); 
        Vec<double,FEAT_DIM1> sv = sv4 + sv1 - sv2 - sv3;

        for(int i=0, cnt=0; i < dim; i++)
        {
            for (int j=i; j < dim; j++) 
            {
                prodM.at<double>(i,j) = pv[cnt++];
                if (j > i)
                {
                    prodM.at<double>(j,i) = prodM.at<double>(i,j);
                }
            }
        }
        sumM = Mat(sv); // sumM should be a dim x 1 matrix
    }
    else
    {
        Vec<double,II_DIM3> pv1 = interpIIprod3(x1, y1);
        Vec<double,II_DIM3> pv2 = interpIIprod3(x2+1.0, y1);
        Vec<double,II_DIM3> pv3 = interpIIprod3(x1, y2+1.0);
        Vec<double,II_DIM3> pv4 = interpIIprod3(x2+1.0, y2+1.0);
        Vec<double,II_DIM3> pv = pv4 + pv1 - pv2 - pv3;
        Vec<double,FEAT_DIM3> sv1 = interpIIsum3(x1, y1);
        Vec<double,FEAT_DIM3> sv2 = interpIIsum3(x2+1.0, y1);
        Vec<double,FEAT_DIM3> sv3 = interpIIsum3(x1, y2+1.0);
        Vec<double,FEAT_DIM3> sv4 = interpIIsum3(x2+1.0, y2+1.0); 
        Vec<double,FEAT_DIM3> sv = sv4 + sv1 - sv2 - sv3;

        for (int i=0, cnt=0; i < dim; i++)
        {
            for (int j=i; j < dim; j++)
            {
                prodM.at<double>(i,j) = pv[cnt++];
                if (j > i)
                {
                    prodM.at<double>(j,i) = prodM.at<double>(i,j);
                }
            }
        }
        sumM = Mat(sv); // sumM should be a dim x 1 matrix
    }
}
/* ------------------------------------------------------------ */

Mat CovImage::covMatrix(double x1, double y1, double x2, double y2,
    double &Npixels){
        Mat prodM, sumM, covmat;
        covComponentMatrices(x1, y1, x2, y2, prodM, sumM, Npixels);
        covmat = prodM/(Npixels-1.0) - sumM*sumM.t()/(Npixels*(Npixels-1.0));
        return covmat;
}
/* ------------------------------------------------------------ */
// crop the rectangle without interpolation 
// void CovImage::covComponentMatrices(int x1, int y1, int x2, int y2,
//                                     Mat &prodM, Mat &sumM, double &Npixels)
// {
//     int L = IIprod.channels();
// 
//     assert(x2 > x1 && y2 > y1 && x1 >= 0 && y1 >= 0 &&
//            x2 < nCols && y2 < nRows);
// 
//     Npixels = (x2-x1+1) *(y2-y1+1);
// 
//     prodM = Mat_<double>(dim,dim,CV_64F);
//     if (L == II_DIM1) {
//         Vec<double,II_DIM1> pv1 = IIprod.at<Vec<double,II_DIM1> >(y1, x1);
//         Vec<double,II_DIM1> pv2 = IIprod.at<Vec<double,II_DIM1> >(y1, x2+1);
//         Vec<double,II_DIM1> pv3 = IIprod.at<Vec<double,II_DIM1> >(y2+1, x1);
//         Vec<double,II_DIM1> pv4 = IIprod.at<Vec<double,II_DIM1> >(y2+1, x2+1);
//         Vec<double,II_DIM1> pv = pv4 + pv1 - pv2 - pv3;
//         Vec<double,FEAT_DIM1> sv1 = IIsum.at<Vec<double,FEAT_DIM1> >(y1, x1);
//         Vec<double,FEAT_DIM1> sv2 = IIsum.at<Vec<double,FEAT_DIM1> >(y1, x2+1);
//         Vec<double,FEAT_DIM1> sv3 = IIsum.at<Vec<double,FEAT_DIM1> >(y2+1, x1);
//         Vec<double,FEAT_DIM1> sv4 = IIsum.at<Vec<double,FEAT_DIM1> >(y2+1, x2+1);
//         Vec<double,FEAT_DIM1> sv = sv4 + sv1 - sv2 - sv3;
// 
//         for (int i=0, cnt=0; i < dim; i++)
//             for (int j=i; j < dim; j++) {
//                 prodM.at<double>(i,j) = pv[cnt++];
//                 if (j > i)
//                     prodM.at<double>(j,i) = prodM.at<double>(i,j);
//             }
//         sumM = Mat(sv); // sumM should be a dim x 1 matrix
//     }
//     else {
//         Vec<double,II_DIM3> pv1 = IIprod.at<Vec<double,II_DIM3> >(y1, x1);
//         Vec<double,II_DIM3> pv2 = IIprod.at<Vec<double,II_DIM3> >(y1, x2+1);
//         Vec<double,II_DIM3> pv3 = IIprod.at<Vec<double,II_DIM3> >(y2+1, x1);
//         Vec<double,II_DIM3> pv4 = IIprod.at<Vec<double,II_DIM3> >(y2+1, x2+1);
//         Vec<double,II_DIM3> pv = pv4 + pv1 - pv2 - pv3;
//         Vec<double,FEAT_DIM3> sv1 = IIsum.at<Vec<double,FEAT_DIM3> >(y1, x1);
//         Vec<double,FEAT_DIM3> sv2 = IIsum.at<Vec<double,FEAT_DIM3> >(y1, x2+1);
//         Vec<double,FEAT_DIM3> sv3 = IIsum.at<Vec<double,FEAT_DIM3> >(y2+1, x1);
//         Vec<double,FEAT_DIM3> sv4 = IIsum.at<Vec<double,FEAT_DIM3> >(y2+1, x2+1);
//         Vec<double,FEAT_DIM3> sv = sv4 + sv1 - sv2 - sv3;
// 
//         for (int i=0, cnt=0; i < dim; i++)
//             for (int j=i; j < dim; j++) {
//                 prodM.at<double>(i,j) = pv[cnt++];
//                 if (j > i)
//                     prodM.at<double>(j,i) = prodM.at<double>(i,j);
//             }
//         sumM = Mat(sv); // sumM should be a dim x 1 matrix
//     }
//  
// }

/* ------------------------------------------------------------ */
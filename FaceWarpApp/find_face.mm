//
//  PHIHarleyStreet.m
//  FaceWarpApp
//
//  Created by Thomas Nickson on 18/09/2015.
//  Copyright © 2015 Phi Research. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "find_face.h"
#import "PHItypes.h"
#include <mutex>

#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing.h>
#include <dlib/image_io.h>
#include <dlib/opencv.h>

#include <opencv2/opencv.hpp>

struct tracker_rect {
    dlib::correlation_tracker tracker;
    dlib::rectangle lastone;
};

//dlib::rectangle dlibRectFromRectangle(PhiRectangle rect) {
//    return dlib::rectangle(rect.left, rect.top, rect.right, rect.bottom);
//}
//
//PhiRectangle rectangleFromDlibRectangle(const dlib::rectangle & rect) {
//    return Rectangle{
//        static_cast<float>(rect.left()),
//        static_cast<float>(rect.top()),
//        static_cast<float>(rect.right()),
//        static_cast<float>(rect.bottom())};
//}
//
//Rectangle operator*(const Rectangle & rect, float scale) {
//    return Rectangle{rect.left * scale, rect.top * scale, rect.right * scale, rect.bottom * scale};
//}

@implementation FaceFinder {
    dlib::shape_predictor predictor;
    dlib::frontal_face_detector detector;
    NSMutableArray * facesAverage;
    NSUInteger movingAverageCount;
    
//    std::vector<dlib::rectangle> faces;
    int retrackAfter;
    int iter;
    std::mutex mtx;
    std::vector<tracker_rect> trackers;
    dlib::rectangle face_loc;
    
    dispatch_queue_t faceQueue;
    
}

-(FaceFinder *)init {
    self = [super init];
    if (self) {
        iter = 0;
        retrackAfter = 3;
        NSString * dat_file = [[NSBundle mainBundle] pathForResource:@"shape_predictor" ofType:@"dat"];
        detector = dlib::get_frontal_face_detector();
        dlib::deserialize(dat_file.UTF8String) >> predictor;
        facesAverage = [[NSMutableArray alloc] init];
        faceQueue = dispatch_queue_create("com.PHI.faceQueue", DISPATCH_QUEUE_CONCURRENT);
        movingAverageCount = 0;
    }
    return self;
};

-(FaceFinder *) initWithRetrack: (int) _retrackAfter {
    self = [self init];
    if (self) {
        retrackAfter = _retrackAfter;
    }
    return self;
}

-(UIImage *) UIImageFromCVMat:(cv::Mat)cvMat
{
    NSData *data = [NSData dataWithBytes:cvMat.data length:cvMat.elemSize()*cvMat.total()];
    CGColorSpaceRef colorSpace;
    
    if (cvMat.elemSize() == 1) {
        colorSpace = CGColorSpaceCreateDeviceGray();
    } else {
        colorSpace = CGColorSpaceCreateDeviceRGB();
    }
    
    CGDataProviderRef provider = CGDataProviderCreateWithCFData((__bridge CFDataRef)data);
    
    // Creating CGImage from cv::Mat
    CGImageRef imageRef = CGImageCreate(cvMat.cols,                                 //width
                                        cvMat.rows,                                 //height
                                        8,                                          //bits per component
                                        8 * cvMat.elemSize(),                       //bits per pixel
                                        cvMat.step[0],                            //bytesPerRow
                                        colorSpace,                                 //colorspace
                                        kCGImageAlphaNone|kCGBitmapByteOrderDefault,// bitmap info
                                        provider,                                   //CGDataProviderRef
                                        NULL,                                       //decode
                                        false,                                      //should interpolate
                                        kCGRenderingIntentDefault                   //intent
                                        );
    
    
    // Getting UIImage from CGImage
    UIImage *finalImage = [UIImage imageWithCGImage:imageRef];
    CGImageRelease(imageRef);
    CGDataProviderRelease(provider);
    CGColorSpaceRelease(colorSpace);
    
    return finalImage;
};

-(NSArray *) facesPointsInBigImage:(CamImage)_bigImg andSmallImage: (CamImage)_smallImg withScale: (int) scale {
    //Convert CamImages into dlib images:
    cv::Mat bigMat(_bigImg.height, _bigImg.width, CV_8UC4, _bigImg.pixels, _bigImg.rowSize);
    dlib::cv_image<dlib::rgb_alpha_pixel> bigImg(bigMat);
    
    cv::Mat smallMatWithA(_smallImg.height, _smallImg.width, CV_8UC4, _smallImg.pixels, _smallImg.rowSize);
    cv::Mat smallMat;
    cv::cvtColor(smallMatWithA, smallMat, CV_BGRA2RGB);
    dlib::cv_image<dlib::rgb_pixel> smallImg(smallMat);
    
    if (iter  == retrackAfter) {
//        std::cout << "Tracker restarted and iter is " << iter << std::endl;
        
        
        // Copy small img data on main thread
        cv::Mat smallMatCopy = smallMat;
        
        // Asynchronously find the faces using dlib's face detector
        dispatch_async(faceQueue, ^{
            dlib::cv_image<dlib::rgb_pixel> smallImgCopy(smallMatCopy);
            std::vector<dlib::rectangle> faces = detector(smallImgCopy);
            
            // Update trackers inside mutex
            mtx.lock();
            trackers.clear();
            for (auto face : faces) {
                dlib::correlation_tracker tracker;
                tracker.start_track(smallImgCopy, face);
                trackers.push_back(tracker_rect{tracker, face});
            }
            mtx.unlock();
            
            iter = 0;
//            std::cout << "Restart complete" << std::endl;
        });
    }
//    std::cout << "Iter is " << iter << std::endl;
    iter++;
    
    // Get rectanges from tracker inside mutex
    std::vector<dlib::rectangle> rects;
    mtx.lock();
    for (auto tr : trackers) {
        tr.tracker.update(smallImg, tr.lastone);
        dlib::rectangle smallRect = tr.tracker.get_position();
        dlib::rectangle faceRect = dlib::rectangle(
                                   static_cast<long>(smallRect.left() * scale),
                                   static_cast<long>(smallRect.top() * scale),
                                   static_cast<long>(smallRect.right() * scale),
                                   static_cast<long>(smallRect.bottom() * scale)
                                   );
        rects.push_back(faceRect);
    }
    mtx.unlock();
    
    
    // Got face points outside mutex
    NSMutableArray * arr = [[NSMutableArray alloc] init];
    for (auto faceRect : rects) {
        NSMutableArray * internalArr = [[NSMutableArray alloc] init];
        dlib::full_object_detection res = predictor(bigImg, faceRect);
        for (int pidx = 0; pidx < res.num_parts(); ++pidx) {
            [internalArr addObject: [NSValue valueWithCGPoint:CGPointMake(
                                                res.part(pidx).x(), res.part(pidx).y()
                                    )]];
        }
        [arr addObject: internalArr];
    }
    return arr;
}

CGPoint CGPointAdd(CGPoint p1, CGPoint p2)
{
    return CGPointMake(p1.x + p2.x, p1.y + p2.y);
}

CGPoint CGPointAdjustScaling(CGPoint p1, double v1)
{
    return CGPointMake(p1.x * v1, p1.y * v1);
}

@end
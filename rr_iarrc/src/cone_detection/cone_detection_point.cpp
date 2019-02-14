#include <ros/ros.h>
#include <ros/publisher.h>
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/Image.h>
#include "std_msgs/Float64MultiArray.h"
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>

using namespace cv;
using namespace std;

cv_bridge::CvImagePtr cv_ptr;
ros::Publisher pub, pub1, pub2;

cv::Mat kernel(int, int);
void cutEnvironment(cv::Mat);
void publishMessage(ros::Publisher, Mat, std::string);
cv::Mat  getCenter(cv::Mat, double);
cv::Mat  cutBodies(cv::Mat, Mat);
cv::Mat  doWatershed(Mat, cv::Mat, Mat);
cv::Mat  drawAndCalc(cv::Mat, Mat);

int blockSky_height, blockWheels_height, blockBumper_height;
int low_H, high_H, low_S, low_V;
int canny_cut_min_threshold;
double percent_max_distance_transform;

double fy = 111; //586.508911;
double real_height = .45; //cm

void img_callback(const sensor_msgs::ImageConstPtr& msg) {
    cv_ptr = cv_bridge::toCvCopy(msg, "bgr8");
    Mat frame = cv_ptr->image;

    cv::Mat hsv_frame, orange_found;
    Mat frame_cut = frame;
    cutEnvironment(frame_cut);

    cv::cvtColor(frame_cut, hsv_frame, cv::COLOR_BGR2HSV);
    cv::inRange(hsv_frame, cv::Scalar(low_H, low_S, low_V), cv::Scalar(high_H, 255, 255), orange_found);
    cv::morphologyEx(orange_found, orange_found, cv::MORPH_OPEN, kernel(2,2));

    Mat detected_bodies = cutBodies(frame_cut, orange_found);

    Mat sure_bodies = doWatershed(frame, orange_found, detected_bodies);

    frame = drawAndCalc(frame, sure_bodies);

//	publish Images
    publishMessage(pub, frame, "bgr8");
    publishMessage(pub1, sure_bodies, "mono8");
}


int main(int argc, char** argv) {
    ros::init(argc, argv, "coneDetection");

    ros::NodeHandle nh;
    ros::NodeHandle nhp("~");
    std::string subscription_node;
    nhp.param("orange_low_H", low_H, 5);
    nhp.param("orange_high_H", high_H, 25);
    nhp.param("orange_low_S", low_S, 140);
    nhp.param("orange_low_V", low_V, 140);

    nhp.param("canny_cut_min_threshold", canny_cut_min_threshold, 35);
    nhp.param("percent_max_distance_transform", percent_max_distance_transform, 0.7);

    nhp.param("blockSky_height", blockSky_height, 220);
    nhp.param("blockWheels_height", blockWheels_height, 200);
    nhp.param("blockBumper_height", blockBumper_height, 200);

    nhp.param("subscription_node", subscription_node, std::string("/camera/image_color_rect"));

    pub = nh.advertise<sensor_msgs::Image>("/overlay_cones_point", 1); //test publish of image
    pub1 = nh.advertise<sensor_msgs::Image>("/cone_point_body_debug", 1);
    pub2 = nh.advertise<std_msgs::Float64MultiArray>("/cone_point_position_data", 1);
    auto img_real = nh.subscribe(subscription_node, 1, img_callback);

    ros::spin();
    return 0;
}


cv::Mat cutBodies(cv::Mat frame_cut, cv::Mat orange_found) {
    Mat frame_gray, detected_edges, detected_bodies;

    GaussianBlur(frame_cut, frame_cut, Size(5,5), 0, 0, BORDER_DEFAULT );
    cv::cvtColor(frame_cut, frame_gray, cv::COLOR_BGR2GRAY );

    bitwise_and(frame_gray, orange_found, frame_gray);

    cv::Canny(frame_gray, detected_edges, canny_cut_min_threshold, canny_cut_min_threshold*3, 3);
    dilate(detected_edges, detected_edges,kernel(2,2));

    floodFill(detected_edges, Point(0,0), cv::Scalar(255));

    bitwise_not(detected_edges, detected_bodies);
    morphologyEx(detected_bodies, detected_bodies, cv::MORPH_OPEN, kernel(3,3));

    return detected_bodies;
}


cv::Mat getCenter(cv::Mat img, double thres_value) {
    Mat dist_transform, thres, dist_mask, mask;
    Mat sure_front = Mat::zeros(img.size(), CV_8UC1);
    vector<vector<Point>> contours1;
    double min, max;

    distanceTransform(img, dist_transform, CV_DIST_L2, 3);
    findContours(img, contours1, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE);

    for( int i = 0; i < contours1.size(); i++ ) {
        mask = Mat::zeros(img.size(), CV_8UC1);
        dist_mask = Scalar::all(0);

        drawContours(mask, contours1, i, 255, CV_FILLED);

        dist_transform.copyTo(dist_mask, mask);

        cv::minMaxLoc(dist_mask, &min, &max);

        threshold(dist_mask, thres, max * thres_value, 255, CV_THRESH_BINARY);
        thres.convertTo(thres, CV_8UC1);
        bitwise_or(thres, sure_front, sure_front);
    }

    return sure_front;
}



cv::Mat doWatershed(cv::Mat frame,cv::Mat orange_found, cv::Mat detected_bodies) {
    Mat sure_back, dist_transform, unknown, markers;

    Mat sure_front = getCenter(detected_bodies, percent_max_distance_transform);
    sure_front.convertTo(sure_front, CV_8UC1);

    dilate(orange_found, sure_back, kernel(3,3));

    subtract(sure_back, sure_front, unknown);

    connectedComponents(sure_front, markers);
    markers += 1;
    markers.setTo(0, unknown == 255);

    watershed(frame, markers);

    Mat lines = Mat::zeros(orange_found.size(), CV_8UC1);
    lines.setTo(Scalar(255), markers == -1);
    rectangle(lines, Point(0,0), Point(frame.cols, frame.rows), Scalar(0), 2);

    dilate(lines, lines, kernel(2,1));
    floodFill(lines, Point(0,0), cv::Scalar(255));

    Mat sure_body;
    bitwise_not(lines, sure_body);

    return sure_body;
}

cv::Mat drawAndCalc(cv::Mat frame, cv::Mat sure_bodies) {
    vector<vector<Point> > contours;
    findContours(sure_bodies, contours, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE);
    vector<Rect> rect( contours.size() );
    std_msgs::Float64MultiArray list;

    for( int i = 0; i < contours.size(); i++ ) {

        rect[i] = boundingRect(contours[i]);

        if (rect[i].height < 20 || rect[i].width < 10) {
            continue;
        }

        Moments m = moments(contours[i], true);
        Point center = Point(m.m10 / m.m00, m.m01 / m.m00);

        double distance = (real_height * fy) / rect[i].height;
        double px_horz_dist = abs(frame.cols/2 - (rect[i].tl().x + rect[i].width/2));
        double horz_dist = distance * px_horz_dist / fy;

        cerr << "Box Height, Mid Dist: (" << rect[i].height << ", " << px_horz_dist << ")";
        cerr << "      X, Y Dist: (" << distance << ", " << horz_dist << ") " << endl;

        Scalar color =  Scalar(0, 255);
        drawContours( frame, contours, i, color, 1, 8, vector<Vec4i>(), 0, Point() );
        rectangle(frame, rect[i].tl(), rect[i].br(), color, 3, 8, 0);
        circle(frame, center, 5,  Scalar(255,0,255), CV_FILLED, 8, 0);
    }

    return frame;
}


cv::Mat kernel(int x, int y) {
    return cv::getStructuringElement(cv::MORPH_RECT,cv::Size(x,y));
}

void cutEnvironment(cv::Mat img) {
    cv::rectangle(img,
                  cv::Point(0,0),
                  cv::Point(img.cols,img.rows / 3 + blockSky_height),
                  cv::Scalar(0,0,0),CV_FILLED);

    cv::rectangle(img,
                  cv::Point(0,img.rows),
                  cv::Point(img.cols,2 * img.rows / 3 + blockWheels_height),
                  cv::Scalar(0,0,0),CV_FILLED);

    cv::rectangle(img,
                  cv::Point(img.cols/3,img.rows),
                  cv::Point(2 * img.cols / 3, 2 * img.rows / 3 + blockBumper_height),
                  cv::Scalar(0,0,0),CV_FILLED);
}


void publishMessage(ros::Publisher pub, Mat img, std::string img_type) {
    if (pub.getNumSubscribers() > 0) {
        sensor_msgs::Image outmsg;
        cv_ptr->image = img;
        cv_ptr->encoding = img_type;
        cv_ptr->toImageMsg(outmsg);
        pub.publish(outmsg);
    }
}


#include <opencv2/opencv.hpp>
#include <opencv2/aruco.hpp>
#include <iostream>
#include <ros/ros.h>
#include <geometry_msgs/Vector3.h>
#include <sensor_msgs/CompressedImage.h>
#include <cv_bridge/cv_bridge.h>
#include <std_msgs/Float32MultiArray.h>
#include <geometry_msgs/Vector3.h>
#include <eigen3/Eigen/Dense> // Make sure to have Eigen library installed

// 28    80cm      7
// 120cm 
// 19              96   -------->X
// diagonal = 72.111cm

#define WAYPOINT_DISTANCE 15


//1920                              
// cv::Mat camera_matrix                                   = (cv::Mat_<double>(3, 3) << 2004.240793, 0.000000, 974.445467, 0.000000, 1972.754783, 591.922041, 0.000000, 0.000000, 1.000000);
// cv::Mat distortion_coefficients                         = (cv::Mat_<double>(1, 5) << -0.459209, 0.259088, -0.000816, -0.000174, 0.000000);

//640 bottom                        
// cv::Mat camera_matrix                                   = (cv::Mat_<double>(3, 3) << 625.282300, 0.000000, 323.520079, 0.000000, 663.259633, 128.107059, 0.000000, 0.000000, 1.000000);
// cv::Mat distortion_coefficients              `           = (cv::Mat_<double>(1, 5) << -0.468410, 0.199118, 0.012586, -0.001849, 0.000000);

//on air sony                       
// cv::Mat camera_matrix                                   = (cv::Mat_<double>(3, 3) << 686.616434, 0.000000, 307.541900, 0.000000, 686.823297, 226.293628, 0.000000, 0.000000, 1.000000);
// cv::Mat distortion_coefficients                         = (cv::Mat_<double>(1, 5) << 0.102440, -0.083527, -0.003444, -0.011869, 0.000000);

// my ios                           
cv::Mat camera_matrix                                   = (cv::Mat_<double>(3, 3) << 962.6223126270734, 0.000000, 637.8400914606574, 0.000000, 965.4675128523231, 359.65179064008913, 0.000000, 0.000000, 1.000000);
cv::Mat distortion_coefficients                         = (cv::Mat_<double>(1, 5) << 0.058319642341059984, -0.0912051019399981, 0.001540828309673785, -0.002404788028852987, 0.000000);

//640 front                         
// cv::Mat camera_matrix                                   = (cv::Mat_<double>(3, 3) << 664.7437142005466, 0.0, 315.703082526844, 0.0, 669.5841391770296, 252.84811434264267, 0.0, 0.0, 1.0);
// cv::Mat distortion_coefficients                         = (cv::Mat_<double>(1, 5) << -0.4812806594873973, 0.2745181609001952, 0.004042548280670333, -0.006039934872833289, 0.0);
float MARKER_SIZE                                       = 16.5;
cv::Ptr<cv::aruco::Dictionary> marker_dict              = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_ARUCO_ORIGINAL);
cv::Ptr<cv::aruco::DetectorParameters> param_markers    = cv::aruco::DetectorParameters::create();
ros::Publisher                                          waypoint_publisher, pixel_publisher;

// D = [-0.4812806594873973, 0.2745181609001952, 0.004042548280670333, -0.006039934872833289, 0.0]
// K = [664.7437142005466, 0.0, 315.703082526844, 0.0, 669.5841391770296, 252.84811434264267, 0.0, 0.0, 1.0]
// R = [1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0]
// P = [583.5906372070312, 0.0, 309.4498820797671, 0.0, 0.0, 623.8204956054688, 255.52599687316615, 0.0, 0.0, 0.0, 1.0, 0.0]

class Aruco {
    public:
        int                                     id;
        float                                   roll, pitch, yaw; //pose
        std::vector<cv::Point2f>                marker_corners;
        cv::Vec3d                               rVec, tVec;
        std::vector<float> world_coordinates;//    = std::vector<float>(3);
        std::vector<float> pivot_coordinates;//    = std::vector<float>(3);
        std::vector<float> closest_coordinates;//  = std::vector<float>(3);
        cv::Mat                                 frame;
        int                                     pix_y, pix_x;
        float                                   a, b, c=72.11, alpha, theta1, theta2, theta3;
        Aruco(int marker_id) {
            id                                  = marker_id;
        }
        void forward() {
            world_coordinates.clear();
            closest_coordinates.clear();
            pivot_coordinates.clear();
            cv::Point2f center(0, 0);
            for (const auto& corner : marker_corners) {
                center += corner;
            }
            center *= 0.25;
            pix_y   = center.y;
            pix_x   = center.x;
            cv::drawFrameAxes(frame, camera_matrix, distortion_coefficients, rVec, tVec, MARKER_SIZE*1.5);
            cv::Mat ret = getCornersInWorldFrame(center, rVec, tVec);
            // for (int j=0; j<3; j++) {
            //     ret.at<double>(j,0) = ret.at<double>(j,0)/100;
            // }
            std::cout << "point: " << ret.t() << std::endl;
            for (int j=0; j<3; j++) {
                world_coordinates.push_back(ret.at<double>(j,0));
            }
            for (int j=0; j<3; j++) {
                if ((j+1)%3!=0) {
                    pivot_coordinates.push_back(ret.at<double>(j,0));
                }
                else {
                    pivot_coordinates.push_back(0);
                }
            }
            if (sqrt(ret.at<double>(0,0)*ret.at<double>(0,0))>WAYPOINT_DISTANCE) {
                if (ret.at<double>(0,0) > 0) {
                    closest_coordinates.push_back(ret.at<double>(0,0) - WAYPOINT_DISTANCE);
                }
                else {
                    closest_coordinates.push_back(ret.at<double>(0,0) + WAYPOINT_DISTANCE);
                }
            }
            else {
                closest_coordinates.push_back(ret.at<double>(0,0));
            }
            if (sqrt(ret.at<double>(1,0)*ret.at<double>(1,0))>WAYPOINT_DISTANCE) {
                if (ret.at<double>(1,0) > 0) {
                    closest_coordinates.push_back(ret.at<double>(1,0) - WAYPOINT_DISTANCE);
                }
                else {
                    closest_coordinates.push_back(ret.at<double>(1,0) + WAYPOINT_DISTANCE);
                }
            }
            else {
                closest_coordinates.push_back(ret.at<double>(1,0));
            }
            if (sqrt(ret.at<double>(1,0)*ret.at<double>(1,0))>WAYPOINT_DISTANCE && sqrt(ret.at<double>(0,0)*ret.at<double>(0,0))>WAYPOINT_DISTANCE && sqrt(ret.at<double>(2,0)*ret.at<double>(2,0))>WAYPOINT_DISTANCE) {
                if (ret.at<double>(2,0) > 0) {
                    closest_coordinates.push_back(ret.at<double>(2,0) - WAYPOINT_DISTANCE);
                }
                else {
                    closest_coordinates.push_back(ret.at<double>(2,0) + WAYPOINT_DISTANCE);
                }
            }
            else {
                closest_coordinates.push_back(ret.at<double>(2,0));
            }
            theta1          = round(((atan2((round(world_coordinates[1])/100), (round(world_coordinates[0])/100)))* 180 / CV_PI)*100)/100;
            theta2          = yaw;
            theta3          = atan2(3,2);
            a               = 0.01*round(sqrt(world_coordinates[0]*world_coordinates[0] + world_coordinates[1]*world_coordinates[1]));
            // theta2          = yaw + theta1;
            // float angleB    = theta3 + CV_PI/2 - theta2;
            b               = sqrt(a*a + c*c + 2*a*c*cos(theta2 + theta3 + theta1));
            alpha           = acos((c*c - b*b - a*a)/(2*a*b));// - theta1;
            std::cout       << "theta1: " << theta1 << std::endl;
        }
    private:
        void euler_from_quaternion(double x, double y, double z, double w, double &roll, double &pitch, double &yaw) {
            // roll (x-axis rotation)
            double sinr_cosp = +2.0 * (w * x + y * z);
            double cosr_cosp = +1.0 - 2.0 * (x * x + y * y);
            roll = atan2(sinr_cosp, cosr_cosp);

            // pitch (y-axis rotation)
            double sinp = +2.0 * (w * y - z * x);
            if (fabs(sinp) >= 1)
                pitch = copysign(M_PI / 2, sinp); // use 90 degrees if out of range
            else
                pitch = asin(sinp);

            // yaw (z-axis rotation)
            double siny_cosp = +2.0 * (w * z + x * y);
            double cosy_cosp = +1.0 - 2.0 * (y * y + z * z);
            yaw = atan((-1*siny_cosp)/(-1*cosy_cosp));
        }
        cv::Mat getCornersInWorldFrame(cv::Point2f center, cv::Vec3d rvec, cv::Vec3d tvec){
            cv::Mat translation_vector = cv::Mat::zeros(3, 1, CV_64F);
            translation_vector.at<double>(0) = tvec[0];
            translation_vector.at<double>(1) = tvec[1];
            translation_vector.at<double>(2) = tvec[2];
            cv::Mat inv_cam_matrix = camera_matrix.inv();
            cv::Mat rot_mat;
            cv::Rodrigues(rvec, rot_mat);
            Eigen::Matrix3d eigen_rotation_matrix;
            eigen_rotation_matrix << rot_mat.at<double>(0, 0), rot_mat.at<double>(0, 1), rot_mat.at<double>(0, 2),
                                        rot_mat.at<double>(1, 0), rot_mat.at<double>(1, 1), rot_mat.at<double>(1, 2),
                                        rot_mat.at<double>(2, 0), rot_mat.at<double>(2, 1), rot_mat.at<double>(2, 2);
            Eigen::Quaterniond quat(eigen_rotation_matrix);
            double transform_rotation_x = quat.x();
            double transform_rotation_y = quat.y();
            double transform_rotation_z = quat.z();
            double transform_rotation_w = quat.w();
            double roll_x, pitch_y, yaw_z;
            euler_from_quaternion(transform_rotation_x, transform_rotation_y, transform_rotation_z, transform_rotation_w, roll_x, pitch_y, yaw_z);
            roll_x = roll_x * 180 / CV_PI;
            pitch_y = pitch_y * 180 / CV_PI;
            yaw_z = yaw_z * 180 / CV_PI;
            std::cout << "roll: " << roll_x << " pitch: " << pitch_y << " yaw: " << yaw_z << std::endl;
            cv::Mat pixels = cv::Mat::zeros(3,1,CV_64F);
            pixels.at<double>(0) = center.x;
            pixels.at<double>(1) = center.y;
            pixels.at<double>(2) = 1;
            cv::Mat temp = (rot_mat.inv())*(inv_cam_matrix*pixels - translation_vector);
            return temp;
        }
};

Aruco                       left_top(28);
Aruco                       left_bottom(19);
Aruco                       right_bottom(96);
Aruco                       right_top(7);
cv_bridge::CvImagePtr       cv_ptr;

void imageCallback(const sensor_msgs::CompressedImageConstPtr& msg) {
    try
    {
        cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::BGR8);
    }
    catch (cv_bridge::Exception& e)
    {
        ROS_ERROR("cv_bridge exception: %s", e.what());
        return;
    }
    // cv::Mat  frame_original = cv_ptr->image;
    cv::Mat  frame= cv_ptr->image;
    // cv::flip(frame_original, frame, -1);
    cv::Mat  gray_frame;
    cv::cvtColor(frame, gray_frame, cv::COLOR_BGR2GRAY);
    std::vector<std::vector<cv::Point2f>>   marker_corners;
    std::vector<int>                        marker_IDs;
    std::vector<std::vector<cv::Point2f>>   reject;
    cv::aruco::detectMarkers(gray_frame, marker_dict, marker_corners, marker_IDs, param_markers, reject);

    if (!marker_IDs.empty()) {
        std::vector<cv::Vec3d> rVec, tVec;
        cv::aruco::estimatePoseSingleMarkers(marker_corners, MARKER_SIZE, camera_matrix, distortion_coefficients, rVec, tVec);
        std_msgs::Float32MultiArray f, p;
        std::vector<std::vector<float>> prev_values(10, std::vector<float> (3));
        left_top.frame      = frame;
        left_bottom.frame   = frame;
        right_bottom.frame  = frame;
        right_top.frame     = frame;
        // std::vector<float> prev_mean
        for(int i=0; i<marker_IDs.size(); i++) {
            // std::cout << "i = " << i << "; ID = " << marker_IDs[i] << std::endl;
            if (marker_IDs[i]==28) {
                left_top.marker_corners         = marker_corners[i];
                left_top.tVec                   = tVec[i];
                left_top.rVec                   = rVec[i];
                left_top.forward();
                f.data.push_back(marker_IDs[i]);
                f.data.push_back(left_top.yaw);
                for (int j=0; j<3; j++) {
                    f.data.push_back(left_top.world_coordinates[j]);
                }
                for (int j=0; j<3; j++) {
                    f.data.push_back(left_top.pivot_coordinates[j]);
                }
                for (int j=0; j<3; j++) {
                    f.data.push_back(left_top.closest_coordinates[j]);
                }
                p.data.push_back(marker_IDs[i]);
                p.data.push_back(left_top.pix_y);
                p.data.push_back(left_top.pix_x);
            }
            else if (marker_IDs[i]==19) {
                left_bottom.marker_corners      = marker_corners[i];
                left_bottom.tVec                = tVec[i];
                left_bottom.rVec                = rVec[i];
                left_bottom.forward();
                f.data.push_back(marker_IDs[i]);
                f.data.push_back(left_bottom.yaw);
                for (int j=0; j<3; j++) {
                    f.data.push_back(left_bottom.world_coordinates[j]);
                }
                for (int j=0; j<3; j++) {
                    f.data.push_back(left_bottom.pivot_coordinates[j]);
                }
                for (int j=0; j<3; j++) {
                    f.data.push_back(left_bottom.closest_coordinates[j]);
                }
                p.data.push_back(marker_IDs[i]);
                p.data.push_back(left_bottom.pix_y);
                p.data.push_back(left_bottom.pix_x);
            }
            else if (marker_IDs[i]==96) {
                right_bottom.marker_corners     = marker_corners[i];
                right_bottom.tVec               = tVec[i];
                right_bottom.rVec               = rVec[i];
                right_bottom.forward();
                f.data.push_back(marker_IDs[i]);
                f.data.push_back(right_bottom.yaw);
                for (int j=0; j<3; j++) {
                    f.data.push_back(right_bottom.world_coordinates[j]);
                }
                for (int j=0; j<3; j++) {
                    f.data.push_back(right_bottom.pivot_coordinates[j]);
                }
                for (int j=0; j<3; j++) {
                    f.data.push_back(right_bottom.closest_coordinates[j]);
                }
                p.data.push_back(marker_IDs[i]);
                p.data.push_back(right_bottom.pix_y);
                p.data.push_back(right_bottom.pix_x);
            }
            else if (marker_IDs[i]==7) {
                right_top.marker_corners        = marker_corners[i];
                right_top.tVec                  = tVec[i];
                right_top.rVec                  = rVec[i];
                right_top.forward();
                f.data.push_back(marker_IDs[i]);
                f.data.push_back(right_top.yaw);
                for (int j=0; j<3; j++) {
                    f.data.push_back(right_top.world_coordinates[j]);
                }
                for (int j=0; j<3; j++) {
                    f.data.push_back(right_top.pivot_coordinates[j]);
                }
                for (int j=0; j<3; j++) {
                    f.data.push_back(right_top.closest_coordinates[j]);
                }
                p.data.push_back(marker_IDs[i]);
                p.data.push_back(right_top.pix_y);
                p.data.push_back(right_top.pix_x);
            }
            else {
                ROS_WARN("INVALID ARUCO ID FOUND");
            }
        }
        
        waypoint_publisher.publish(f);
        pixel_publisher.publish(p);
    }
    cv::imshow("frame", frame);
    cv::waitKey(1);
}

int main(int argc, char** argv) {
    ros::init(argc, argv, "localizer_using_aruco");
    ros::NodeHandle nh;
    ros::Subscriber image_subscriber    = nh.subscribe("/camera_down/image_raw/compressed", 1, imageCallback);
    waypoint_publisher                  = nh.advertise<std_msgs::Float32MultiArray>("/aruco/waypoints", 1);
    pixel_publisher                     = nh.advertise<std_msgs::Float32MultiArray>("/aruco/pixels", 1);
    ros::spin();
}
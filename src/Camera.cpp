#include "Camera.h"
#include "opencv4/opencv2/opencv.hpp"

std::vector<Pointf> Camera::getProj(Pointf center, const std::vector<Pointf>& v)
{
    cv::Point3f vec = { pos.x - center.x, pos.y - center.y, pos.z - center.z };
    cv::Point3f vecz = { 0, 0, 1 };

    float cos_theta = vec.dot(vecz) / (cv::norm(vec) * cv::norm(vecz));
    auto theta = acos(cos_theta);

    auto asix = vec.cross(vecz);
    asix = asix / cv::norm(asix);
    asix =asix* theta;
    cv::Mat rvec = cv::Mat(3, 1, CV_32F);
    rvec.at<float>(0, 0) = asix.x;
    rvec.at<float>(1, 0) = asix.y;
    rvec.at<float>(2, 0) = asix.z;
    cv::Mat tvec = cv::Mat(3, 1, CV_32F);
    tvec.at<float>(0, 0) = pos.x;
    tvec.at<float>(1, 0) = pos.y;
    tvec.at<float>(2, 0) = pos.z;

    std::vector<cv::Point3f> vf;
    for (auto& p : v)
    {
        vf.push_back({ p.x, p.y, p.z });
    }
    std::vector<cv::Point2f> v_out;

    cv::Mat cameraMatrix = cv::Mat::eye(3, 3, CV_32F);
    cameraMatrix.at<float>(0, 0) = 200;
    cameraMatrix.at<float>(1, 1) = 200;
    cameraMatrix.at<float>(0, 2) = 100;
    cameraMatrix.at<float>(1, 2) = 100;

    cv::projectPoints(vf, rvec, tvec, cameraMatrix, cv::Mat(), v_out);
    std::vector<Pointf> v_out2;
    for (auto& p : v_out)
    {
        v_out2.push_back({ p.x, p.y });
    }
    return v_out2;
}

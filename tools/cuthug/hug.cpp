#include <cmath>
#include <cstdio>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>
#include <algorithm>

#include <windows.h>
#include <corecrt_io.h>
#include <direct.h>
#include <print>


#include "opencv2/opencv.hpp"

void autocrop(cv::Mat& m)
{
    int minX = m.cols - 1;
    int minY = m.rows - 1;
    int maxX = 0;
    int maxY = 0;
    for (int i = 0; i < m.rows; i++)
    {
        for (int j = 0; j < m.cols; j++)
        {
            if (m.at<cv::Vec3b>(i, j)[0] != 0 || m.at<cv::Vec3b>(i, j)[1] != 0 || m.at<cv::Vec3b>(i, j)[2] != 0)
            {
                minX = std::min(minX, j);
                minY = std::min(minY, i);
                maxX = std::max(maxX, j);
                maxY = std::max(maxY, i);
            }
        }
    }
    cv::Rect rect(minX, minY, maxX - minX + 1, maxY - minY + 1);
    m = m(rect);
}

//将一张大图切割成8*8的小图

int cut_huge_map()
{
    cv::Mat m = cv::imread("map.png", -1);
    //autocrop(m);
    cv::Mat m1 = cv::Mat::zeros(8640,17280, CV_8UC4);
    m.copyTo(m1(cv::Rect(0, 0, m.cols, m.rows)));
    m = m1;
    size_t size = 8;
    int width = m.cols / size;
    int height = m.rows / size;
    int k = 0;
    for (int i = 0; i < size; i++)
    {
        for (int j = 0; j < size; j++)
        {
            cv::Mat m1 = m(cv::Rect(j * width, i * height, width, height));
            double minv, maxv;
            cv::minMaxLoc(m1, &minv, &maxv);
            if (minv != maxv)
            {
                cv::imwrite(std::format("{}.png", k), m1);
                std::print("{}: {},{}\n", k, minv, maxv);
            }
            k++;
        }
    }
    return 0;
}

int main()
{
    cut_huge_map();
    return 0;
}
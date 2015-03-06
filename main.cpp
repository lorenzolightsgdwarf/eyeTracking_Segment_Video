/* 
 * File:   main.cpp
 * Author: chili
 *
 * Created on March 2, 2015, 8:56 AM
 */

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <string>
#include <opencv2/highgui/highgui.hpp>
#include <chilitags/chilitags.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#define START_TAG 1
#define STOP_TAG 2
#define FIX_FILE_STARTCOL 1
#define FIX_FILE_STOPCOL 3


using namespace std;

/*
 * Remember:
 * 
 * Export the fixation file from the event statistics panel of begaze, 
 * otherwise the timestamp will be in unix epoch format.
 * 
 * BeGaze Fixation files use CRLF at the end. Getline reads everything
 * before the LF, so CR is included and needs to be discarded.
 * 
 * The raw video file has approximately 1 sec delay with the fixation file.
 * Hence, first you need to export the videos from begaze.
 * 
 * Use wmv3, since xvid doesn't carry framerate info.    
 * 
 * 
 * 
 * argv[1] video
 * argv[2] event file
 * argv[3] starting number (optional)
 * argv[4] sync image
 */
int main(int argc, char** argv) {

    for (int i = 0; i < argc; i++)
        cout << argv[i] << "\n";

    cv::VideoCapture inputVideo(argv[1]);

    if (!inputVideo.isOpened()) {
        cerr << "Video open failed:" << argv[1];
        return -1;
    }

    ifstream inputEvent(argv[2]);

    if (!inputEvent.is_open()) {
        cerr << "Event open failed:" << argv[2];
        return -1;
    }

    ofstream timestampFile("Timestamp_" + boost::lexical_cast<string>(argv[2]));
    timestampFile << "Trial,StartTime,StopTime\n";

    int currentTrial = 1;
    if (argc == 4)
        currentTrial = atoi(argv[3]);

    chilitags::Chilitags detector;
    detector.setFilter(0, 0.);
    detector.setPerformance(chilitags::Chilitags::ROBUST);

    cv::Mat frame;
    std::map<int, chilitags::Quad> tags;
    bool start_detected = false;
    long startTime;
    cv::VideoWriter videoOutput;
    string line, header;
    getline(inputEvent, header);
    getline(inputEvent, line);
    cv::Mat syncImg = cv::imread(argv[4], 1);
    double syncTimestamp;
    bool sync = false;
    while (inputVideo.read(frame)) {
        if (!sync) {
            cv::Mat diff;
            cv::absdiff(syncImg, frame, diff);
            vector<cv::Mat> channels(3);
            cv::split(diff, channels);
            if (cv::countNonZero(channels[0]) == 0 && cv::countNonZero(channels[1]) == 0
                    && cv::countNonZero(channels[2]) == 0) {
                syncTimestamp = inputVideo.get(CV_CAP_PROP_POS_MSEC);
                sync = true;
            } else
                cout << "Sync not found\n";
        }
        if (!sync) continue;
        tags = detector.find(frame, chilitags::Chilitags::DETECT_ONLY);
        if (tags.count(START_TAG) > 0) {
            start_detected = true;
            //open video writer
            if (videoOutput.isOpened())
                videoOutput.release();
            startTime = inputVideo.get(CV_CAP_PROP_POS_MSEC);
            //cout<<"Start Time"<<startTime<<"\n";
            string name = argv[1];
            name = "Trial_" + boost::lexical_cast<string>(currentTrial) + "_" + name;

            videoOutput.open(name, CV_FOURCC('P', 'I', 'M', '1'), inputVideo.get(CV_CAP_PROP_FPS),
                    cv::Size(inputVideo.get(CV_CAP_PROP_FRAME_WIDTH), inputVideo.get(CV_CAP_PROP_FRAME_HEIGHT)),
                    true);

            videoOutput << frame;

        } else if (tags.count(STOP_TAG) > 0 && start_detected) {
            videoOutput << frame;
            start_detected = false;
            //close video
            videoOutput.release();
            //write event file
            long stopTime = (long) inputVideo.get(CV_CAP_PROP_POS_MSEC);
            //cout<<"Stop Time"<<stopTime<<"\n";
            timestampFile << currentTrial << "," << startTime << "," << stopTime << "\n";
            string name = argv[2];
            name = "Trial_" + boost::lexical_cast<string>(currentTrial) + '_' + name;
            ofstream outputEvent(name);
            //last digits are a comma and cr
            outputEvent << header.substr(0, header.length() - 2) << "\n";
            do {
                vector <std::string> fields;
                boost::split(fields, line, boost::is_any_of(","));
                double start = boost::lexical_cast<long>(fields[FIX_FILE_STARTCOL]);
                if (start > stopTime) {
                    outputEvent.close();
                    break;
                } else if (start + syncTimestamp >= startTime) {
                    for (int i = 0; i < fields.size() - 1; i++) {
                        if (i != 0)
                            outputEvent << ",";
                        if (i == FIX_FILE_STARTCOL || i == FIX_FILE_STOPCOL) {
                            long newvalue = boost::lexical_cast<long>(fields[i]) + syncTimestamp - startTime + 1000 / inputVideo.get(CV_CAP_PROP_FPS);
                            outputEvent << boost::lexical_cast<std::string>(newvalue);
                        } else
                            outputEvent << fields[i];
                    }
                    outputEvent << "\n";
                }
            } while ((getline(inputEvent, line)));
            currentTrial++;

        } else if (start_detected) {
            videoOutput << frame;
        }
    }

    inputVideo.release();
    inputEvent.close();
    timestampFile.close();

    return 0;
}


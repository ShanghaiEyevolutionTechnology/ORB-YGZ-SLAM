#include <iostream>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <chrono>

#include "opencv2/opencv.hpp"

//OrbSLAM2 heaer
#include <System.h>

//Evo SDK header
#include "evo_stereocamera.h"//stereo camera
#include "evo_matconverter.h"//evo Mat converter


bool genYaml(evo::bino::StereoParameters param, evo::bino::Resolution_FPS resfps) {
	ofstream fs("leadsense.yaml", ios_base::out);
	if (!fs.is_open()) {
		std::cerr << "error to open leadsense.yaml" << std::endl;
		return false;
	}
	fs << "%YAML:1.0" << std::endl << "---" << std::endl << std::endl;

	fs << "Camera.fx: " << param.leftCam.focal.x << std::endl;
	fs << "Camera.fy: " << param.leftCam.focal.y << std::endl;
	fs << "Camera.cx: " << param.leftCam.center.x << std::endl;
	fs << "Camera.cy: " << param.leftCam.center.y << std::endl;
	fs << "Camera.k1: " << 0.0 << std::endl;
	fs << "Camera.k2: " << 0.0 << std::endl;
	fs << "Camera.p1: " << 0.0 << std::endl;
	fs << "Camera.p2: " << 0.0 << std::endl;
	fs << "Camera.width: " << resfps.width << std::endl;
	fs << "Camera.height: " << resfps.height << std::endl;
	// Camera frames per second 
	fs << "Camera.fps: " << resfps.fps << std::endl;
	// stereo baseline[m] times fx
	fs << "Camera.bf: " << param.baseline() / 1000 * param.leftCam.focal.x << std::endl;
	// Color order of the images (0: BGR, 1: RGB. It is ignored if images are grayscale)
	fs << "Camera.RGB: " << 0 << std::endl;

	// Close/Far threshold. Baseline times.
	fs << "ThDepth: " << 48 << std::endl << std::endl;

	//--------------------------------------------------------------------------------------------
	// ORB Parameters
	//--------------------------------------------------------------------------------------------
	// ORB Extractor: Number of features per image
	fs << "ORBextractor.nFeatures: " << 1000 << std::endl;
	// ORB Extractor: Scale factor between levels in the scale pyramid 	
	fs << "ORBextractor.scaleFactor: " << 1.2 << std::endl;
	// ORB Extractor: Number of levels in the scale pyramid	
	fs << "ORBextractor.nLevels: " << 8 << std::endl;
	// ORB Extractor: Fast threshold
	// Image is divided in a grid. At each cell FAST are extracted imposing a minimum response.
	// Firstly we impose iniThFAST. If no corners are detected we impose a lower value minThFAST
	// You can lower these values if your images have low contrast			
	fs << "ORBextractor.iniThFAST: " << 20 << std::endl;
	fs << "ORBextractor.minThFAST: " << 7 << std::endl << std::endl;

	//--------------------------------------------------------------------------------------------
	// Viewer Parameters
	//--------------------------------------------------------------------------------------------
	fs << "Viewer.KeyFrameSize: " << 0.6 << std::endl;
	fs << "Viewer.KeyFrameLineWidth: " << 2 << std::endl;
	fs << "Viewer.GraphLineWidth: " << 1 << std::endl;
	fs << "Viewer.PointSize: " << 2 << std::endl;
	fs << "Viewer.CameraSize: " << 0.7 << std::endl;
	fs << "Viewer.CameraLineWidth: " << 3 << std::endl;
	fs << "Viewer.ViewpointX: " << 0 << std::endl;
	fs << "Viewer.ViewpointY: " << -100 << std::endl;
	fs << "Viewer.ViewpointZ: " << -0.1 << std::endl;
	fs << "Viewer.ViewpointF: " << 2000 << std::endl << std::endl;
	fs.close();
	return true;
}

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		std::cerr << std::endl << "Usage: ./leadsense_imu path_to_vocabulary [400(default)|800|.evo file]" << std::endl;
		return 1;
	}
	
	evo::bino::StereoCamera camera;
	evo::bino::GrabParameters grab_parameters;
	grab_parameters.do_rectify = true;
	grab_parameters.calc_disparity = false;
	int width, height;

	int cnt = 0, limit = 100;
	double tempt = 0;
	
	evo::RESULT_CODE res;

	//open camera
	if (argc > 2)
	{
		int res_n = atoi(argv[2]);
		if (res_n == 400)
		{
			res = camera.open(evo::bino::RESOLUTION_FPS_MODE_SD400_30);
		}
		else if (res_n == 800)
		{	
			res = camera.open(evo::bino::RESOLUTION_FPS_MODE_HD800_30);
		}
		else
		{		
			res = camera.open(argv[2]);
		}
	}
	else
	{
		res = camera.open(evo::bino::RESOLUTION_FPS_MODE_SD400_30);
	}
	std::cout << "camera open: " << evo::result_code2str(res) << std::endl;

	if (res == evo::RESULT_CODE_OK)//open camera successed
	{
		//show image size
		width = camera.getImageSizeFPS().width;
		height = camera.getImageSizeFPS().height;
		std::cout << "image width:" << width << ", height:" << height << std::endl;

		if (genYaml(camera.getStereoParameters(true), camera.getImageSizeFPS()) == false) {
			return 2;
		}
		
		camera.setIMUDataType(evo::imu::IMU_DATA_TYPE_RAW_CALIBRATED);
		camera.setIMUDataRetrieveMode(evo::imu::IMU_DATA_RETRIEVE_MODE_NEWEST_IMAGE);
		res = camera.startRetrieveIMU();
		if (res != evo::RESULT_CODE_OK)
		{		
			std::cerr << "start retrieve IMU failed.. "  << evo::result_code2str(res) << std::endl;
			return 3;
		}

		// Create SLAM system. It initializes all system threads and gets ready to process frames.
		ygz::System SLAM(argv[1], "leadsense.yaml", ygz::System::MONOCULAR, true);

		std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();

		//running flag
		bool running = true;
		//main loop
		while (running)
		{
			// Get frames and launch the computation
			res = camera.grab(grab_parameters);
			if (res == evo::RESULT_CODE_OK)
			{
				std::vector<evo::imu::IMUData> imu_data = camera.retrieveIMUData();
				
				evo::Mat<unsigned char> evo_left = camera.retrieveImage(evo::bino::SIDE_LEFT);
				
				//Mat convert
				cv::Mat imLeft = evoMat2cvMat(evo_left);

				double tframe = (double)camera.getCurrentFrameTimeCode() / 1000000000;

				// Pass the data to the SLAM system
				if (imu_data.size() > 0)
				{
					std::vector<ygz::IMUData> vimu;
					for (int i = 0; i < imu_data.size(); i++)
		    		{
		    			ygz::IMUData data(imu_data.at(i).gyro_calibrated[0], imu_data.at(i).gyro_calibrated[1], imu_data.at(i).gyro_calibrated[2], imu_data.at(i).accel_calibrated[0], imu_data.at(i).accel_calibrated[1], imu_data.at(i).accel_calibrated[2], (double)imu_data.at(i).timestamp / 1000000000);
		    			vimu.push_back(data);
		    		}
					cv::Mat pose = SLAM.TrackMonoVI(imLeft, vimu, tframe);
				}
				else
				{
					cv::Mat pose = SLAM.TrackMonocular(imLeft, tframe);
				}
				
				double ttrack = std::chrono::duration_cast<std::chrono::duration<double> >(std::chrono::steady_clock::now() - t1).count();
				t1 = std::chrono::steady_clock::now();

				cnt++;
				tempt += ttrack;
				if (cnt == limit) {
					cout << "avg loop time:" << tempt / limit << endl;
					cnt = 0;
					tempt = 0;
				}
			}
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
		camera.close();
		// Stop all threads
		SLAM.Shutdown();
	}

	return 0;
}


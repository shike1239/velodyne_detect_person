#include "ros/ros.h"
#include "std_msgs/String.h"
#include <sensor_msgs/PointCloud2.h>
#include <pcl/io/io.h>
#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/segmentation/extract_clusters.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/console/print.h>
#include <pcl/console/parse.h>
#include <pcl/console/time.h>
#include <pcl/common/boost.h>
#include <vector>
#include <pcl_ros/point_cloud.h>
#include "std_msgs/String.h"
#include <boost/shared_ptr.hpp>
#include <pcl/search/impl/kdtree.hpp>
#include "velodyne_detect_person/pointCloudVector.h"
#include <pcl/filters/passthrough.h>
#include <pcl/segmentation/region_growing.h>
#include <pcl/features/normal_3d.h>


typedef pcl::PointCloud<pcl::PointXYZ> PointCloud;
using namespace pcl;
using namespace pcl::io;
using namespace pcl::console;


class ExtractClusters
{
  protected:
    ros::NodeHandle n;
		ros::Time begin;
		int n_published_msgs;
  public:
    ros::Publisher pub = n.advertise<velodyne_detect_person::pointCloudVector> ("scene_clusters", 1);
    ros::Publisher pub2 = n.advertise<sensor_msgs::PointCloud2> ("clustersCloud", 1);
    ros::Subscriber sub;

	void extractClustersCallback(const boost::shared_ptr<sensor_msgs::PointCloud2>& inputCloud)
	{
	
		//Convert ros PointCloud2 to pcl::PointCloud<pcl::pointXYZ>::Ptr
	  pcl::PCLPointCloud2 pcl_pc2;
	  pcl_conversions::toPCL(*inputCloud, pcl_pc2);
	  pcl::PointCloud<pcl::PointXYZ>::Ptr inputPclCloud(new pcl::PointCloud<pcl::PointXYZ>);
	  pcl::fromPCLPointCloud2(pcl_pc2,*inputPclCloud);
	  
		//Filter cloud to remove floor, ceiling and very far readings (velodyne frame)
		pcl::PointCloud<pcl::PointXYZ>::Ptr filteredInputPclCloud(new pcl::PointCloud<pcl::PointXYZ>);
		pcl::PassThrough<pcl::PointXYZ> pass;
 /* 	pass.setInputCloud (inputPclCloud);
  	pass.setFilterFieldName ("z");
  	pass.setFilterLimits (-1.00, -0.85);
  	pass.setFilterLimitsNegative (true);
  	pass.filter (*filteredInputPclCloud);*/

    pass.setInputCloud (inputPclCloud);
  	pass.setFilterFieldName ("z");
  	pass.setFilterLimits (-0.85, 1.5);
  	pass.setFilterLimitsNegative (false);
  	pass.filter (*filteredInputPclCloud);
  	
  	pass.setInputCloud (filteredInputPclCloud);
  	pass.setFilterFieldName ("x");
  	pass.setFilterLimits (5.0, 20.0);
  	pass.setFilterLimitsNegative (true);
  	pass.filter (*filteredInputPclCloud);	
  	 
 /* 	pass.setInputCloud (filteredInputPclCloud);
  	pass.setFilterFieldName ("z");
  	pass.setFilterLimits (1.5, 6.0);
  	pass.setFilterLimitsNegative (true);
  	pass.filter (*filteredInputPclCloud); */
  	
		/*
  	pass.setInputCloud (filteredInputPclCloud);
  	pass.setFilterFieldName ("x");
  	pass.setFilterLimits (-3.0, -1.2);
  	pass.setFilterLimitsNegative (true);
  	pass.filter (*filteredInputPclCloud); 
		*/
	  
	  sensor_msgs::PointCloud2::Ptr clustersCloudRos (new sensor_msgs::PointCloud2);
	  pcl::PointCloud<pcl::PointXYZ>::Ptr clustersCloud (new pcl::PointCloud<pcl::PointXYZ>);
	  sensor_msgs::PointCloud2::Ptr auxiliarCluster (new sensor_msgs::PointCloud2);
	    
	  //KdTree object for the clustering search method 
	  pcl::search::KdTree<pcl::PointXYZ>::Ptr tree (new pcl::search::KdTree<pcl::PointXYZ>);
	  tree->setInputCloud (filteredInputPclCloud);
	  
	  //Perform clustering
	  
	  //Object for storing the normals.
		pcl::PointCloud<pcl::Normal>::Ptr normals(new pcl::PointCloud<pcl::Normal>);
		
		//Estimate the normals.		
		pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> normalEstimation;
		normalEstimation.setInputCloud(filteredInputPclCloud);
		normalEstimation.setRadiusSearch(0.03);
		normalEstimation.setSearchMethod(tree);
		normalEstimation.compute(*normals);
	 
		// Region growing clustering object.
		pcl::RegionGrowing<pcl::PointXYZ, pcl::Normal> clustering;
		clustering.setMinClusterSize(100);
		clustering.setMaxClusterSize(10000);
		clustering.setSearchMethod(tree);
		clustering.setNumberOfNeighbours(30);
		clustering.setInputCloud(filteredInputPclCloud);
		clustering.setInputNormals(normals);
		// Set the angle in radians that will be the smoothness threshold
		// (the maximum allowable deviation of the normals).
		clustering.setSmoothnessThreshold(7.0 / 180.0 * M_PI); // 7 degrees.
		// Set the curvature threshold. The disparity between curvatures will be
		// tested after the normal deviation check has passed.
		clustering.setCurvatureThreshold(1.0);
	 
		std::vector<pcl::PointIndices> cluster_indices;
		clustering.extract(cluster_indices);
		
		// For every cluster...
		int currentClusterNum = 1;
		for (std::vector<pcl::PointIndices>::const_iterator i = cluster_indices.begin(); i != cluster_indices.end(); ++i)
		{
			// ...add all its points to a new cloud...
			pcl::PointCloud<pcl::PointXYZ>::Ptr cluster(new pcl::PointCloud<pcl::PointXYZ>);
			for (std::vector<int>::const_iterator point = i->indices.begin(); point != i->indices.end(); point++)
				cluster->points.push_back(filteredInputPclCloud->points[*point]);
			cluster->width = cluster->points.size();
			cluster->height = 1;
			cluster->is_dense = true;
	 
			// ...and save it to disk.
			if (cluster->points.size() <= 0)
				break;
			std::cout << "Cluster " << currentClusterNum << " has " << cluster->points.size() << " points." << std::endl;
			std::string fileName = "cluster" + boost::to_string(currentClusterNum) + ".pcd";
			pcl::io::savePCDFileASCII(fileName, *cluster);
	 
			currentClusterNum++;
		}
	  
	  
	  /*
	  std::vector<pcl::PointIndices> cluster_indices;
	  pcl::EuclideanClusterExtraction<pcl::PointXYZ> ec;
	  ec.setClusterTolerance (0.2);
	  ec.setMinClusterSize (100);
	  ec.setMaxClusterSize (15000);
	  ec.setSearchMethod (tree);
	  ec.setInputCloud (filteredInputPclCloud);
		ros::Time begin_clustering = ros::Time::now ();
	  ec.extract (cluster_indices);
		double clustering_time = (ros::Time::now () - begin_clustering).toSec ();
		ROS_INFO ("%f secs for clustering (%d clusters).", clustering_time, (int) cluster_indices.size ());
	  */
	  
	  
	  
	  /*Extract each cluster and store them in:
	  		- clusterPointClouds: pointClouds vector. Each element contains a cluster. Not viewable
	  		- clustersCloud: pointcloud containing every cluster. Viewable in rviz
	  */
	  std::vector<pcl::PointIndices>::const_iterator it;
	  //std::vector<int>::const_iterator pit;
	  velodyne_detect_person::pointCloudVector clusterPointClouds;
	  
	  for(it = cluster_indices.begin(); it != cluster_indices.end(); ++it) {
		pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_cluster (new pcl::PointCloud<pcl::PointXYZ>);
                pcl::copyPointCloud (*filteredInputPclCloud, it->indices, *cloud_cluster);
		//for(pit = it->indices.begin(); pit != it->indices.end(); pit++) {
		//  cloud_cluster->points.push_back(filteredInputPclCloud->points[*pit]);    
		//}
		pcl::toROSMsg (*cloud_cluster , *auxiliarCluster);
		auxiliarCluster->header.frame_id = "/velodyne";
	  	auxiliarCluster->header.stamp = ros::Time::now();
		clusterPointClouds.pointCloudVector.push_back(*auxiliarCluster);
		
		*clustersCloud += *cloud_cluster;
	  }
	 
	  pcl::toROSMsg (*clustersCloud , *clustersCloudRos);
	  clustersCloudRos->header.frame_id = "/velodyne";
	  clustersCloudRos->header.stamp = ros::Time::now();
	  
	  pub.publish (clusterPointClouds);
	  pub2.publish (*clustersCloudRos);
		n_published_msgs++;
		double elapsed_time = (ros::Time::now () - begin).toSec ();
		ROS_INFO("Cluster publish freq: %f msgs/s - %d msgs in %f secs.", (float) n_published_msgs / elapsed_time, n_published_msgs, elapsed_time);

	}
	
	ExtractClusters()
    {
      sub = n.subscribe("velodyne_points", 1, &ExtractClusters::extractClustersCallback, this);
			begin = ros::Time::now();
			n_published_msgs = 0;
    }
};
	

int main(int argc, char **argv)
{
  ros::init(argc, argv, "extract_clusters");
  ExtractClusters exC;

  ros::spin();

  return 0;
}

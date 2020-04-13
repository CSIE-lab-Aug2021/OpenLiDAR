
#include <vector>
#include <string>
#include <sstream>

#include <glm/glm.hpp>

#include <pcl/filters/voxel_grid.h>
#include <pcl/kdtree/kdtree_flann.h>
#include <pcl/features/normal_3d.h>
#include <pcl/kdtree/kdtree_flann.h>
#include <pcl/surface/mls.h>
#include <pcl/filters/statistical_outlier_removal.h>

#include "textOps.h"
#include "fileOps.h"
#include "../apps/3dscan/export.h"

int main(int argc, char **argv){

    // Formats
    std::string in_filename = "pcl";
    std::string in_extension = "ply";
    std::string out_filename = "out";
    std::vector<std::string> out_formats;

    // Operations
    float   voxel = 0.0f;
    float   bSOR = false;
    bool    bMLS = false;
    bool bNormal = false;

    for (int i = 1; i < argc ; i++) {
        std::string argument = std::string(argv[i]);

        if ( (std::string(argv[i]) == "-i") ) {
            if (++i < argc)
                in_filename = std::string(argv[i]);
        }
        else if ( std::string(argv[i]) == "-o" ) {
            if (++i < argc)
                out_filename = argv[i];
        }
        else if ( std::string(argv[i]) == "--formats" ) {
            if (++i < argc) {
                out_formats.clear();
                out_formats = split(std::string(argv[i]), ',', true);
            }
        }
        else if ( std::string(argv[i]) == "--voxel" ) {
            if (++i < argc)
                voxel = toFloat(std::string(argv[i])) * 0.01;
        }
        else if ( std::string(argv[i]) == "--sor" )
            bSOR = true;
        else if ( std::string(argv[i]) == "--normals" )
            bNormal = true;
        else if ( std::string(argv[i]) == "--mls" )
            bMLS = true;
    }

    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud (new pcl::PointCloud<pcl::PointXYZ>);

    in_extension = getExt(in_filename);
    std::string out_extension = getExt(out_filename);
    if (out_extension.size() == 3) {
        out_formats.push_back(out_extension);
        out_filename = split(out_filename,'.',true)[0];
    }

    if (in_extension == "pcd") {
        if (pcl::io::loadPCDFile<pcl::PointXYZ> (in_filename.c_str(), *cloud) == -1) {
            PCL_ERROR ("Couldn't read file &s\n", in_filename);
            return (-1);
        }
    }
    else if (in_extension == "ply") {
        if (pcl::io::loadPLYFile<pcl::PointXYZ> (in_filename.c_str(), *cloud) == -1) {
            PCL_ERROR ("Couldn't read file &s\n", in_filename);
            return (-1);
        }
    }

    if (voxel > 0.0) {
        std::cout << "Filtering into a voxel grid" << std::endl;
        pcl::VoxelGrid<pcl::PointXYZ> vg_filter;
        vg_filter.setInputCloud (cloud);
        vg_filter.setLeafSize (voxel, voxel, voxel);
        pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_filtered( new pcl::PointCloud<pcl::PointXYZ> );
        vg_filter.filter(*cloud_filtered);
        *cloud = *cloud_filtered;
    }

    if (bSOR) {
        std::cout << "Remove Statical Outlier" << std::endl;
        pcl::StatisticalOutlierRemoval<pcl::PointXYZ> sor_filter;
        sor_filter.setInputCloud (cloud);
        sor_filter.setMeanK (20);
        sor_filter.setStddevMulThresh (1.0);
        sor_filter.setNegative (false);
        pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_filtered( new pcl::PointCloud<pcl::PointXYZ> );
        sor_filter.filter (*cloud_filtered);
        *cloud = *cloud_filtered;
    }
    
    if (bMLS) {
        std::cout << "Moving Least Squares filter" << std::endl;

        // Create a KD-Tree
        pcl::search::KdTree<pcl::PointXYZ>::Ptr tree (new pcl::search::KdTree<pcl::PointXYZ>);

        // Output has the PointNormal type in order to store the normals calculated by MLS
        pcl::PointCloud<pcl::PointNormal> mls_points;

        // Init object (second point type is for the normals, even if unused)
        pcl::MovingLeastSquares<pcl::PointXYZ, pcl::PointNormal> mls;
        
        mls.setComputeNormals (bNormal);

        // Set parameters
        mls.setInputCloud (cloud);
        mls.setPolynomialOrder (2);
        mls.setSearchMethod (tree);
        mls.setSearchRadius (0.03);

        // Reconstruct
        mls.process (mls_points);

        savePointCloud(out_filename, out_formats, mls_points);
    }
    else if (bNormal) {
        std::cout << "Estimating normals" << std::endl;

        // // Normal estimation*
        pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> n;
        pcl::PointCloud<pcl::Normal>::Ptr normals (new pcl::PointCloud<pcl::Normal>);
        pcl::search::KdTree<pcl::PointXYZ>::Ptr tree (new pcl::search::KdTree<pcl::PointXYZ>);
        tree->setInputCloud (cloud);
        n.setInputCloud (cloud);
        n.setSearchMethod (tree);
        n.setKSearch (10);
        n.compute (*normals);

        // Concatenate the XYZ and normal fields*
        pcl::PointCloud<pcl::PointNormal>::Ptr cloud_with_normals (new pcl::PointCloud<pcl::PointNormal>);
        pcl::concatenateFields (*cloud, *normals, *cloud_with_normals);

        savePointCloud(out_filename, out_formats, *cloud_with_normals);
    }
    else
        savePointCloud(out_filename, out_formats, *cloud);

    return 0;
}

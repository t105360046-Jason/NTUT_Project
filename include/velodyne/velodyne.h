#ifndef VELODYNE_H_
#define VELODYNE_H_

#define HAVE_BOOST
#define HAVE_PCAP
#define HAVE_FAST_PCAP
#define MAX_QUEUE_SIZE 10

#include <vector>
#include <pcl/point_cloud.h>
#include <Eigen/src/Core/Matrix.h>
#ifndef MAX_QUEUE_SIZE
#include "../../3rdparty/VelodyneCapture/.h"
#else
#include "../../3rdparty/VelodyneCapture/VelodyneCapture_modified.h"
#endif

namespace velodyne {

    class VLP16 : public VLP16Capture {
        public:
            int64_t frameNumber;
            double offsetAzimuth;
            double offsetX;
            double offsetY;
            double offsetZ;
            std::string filename;
            std::vector<Laser> lasers;
            bool transform;
            Eigen::Matrix4f transformMatrix;

            VLP16() : VLP16Capture(), frameNumber(-1), offsetX(0.0), offsetY(0.0), offsetZ(0.0), offsetAzimuth(0.0), transform(false), transformMatrix(Eigen::Matrix4f::Identity()) {};

            const bool open( const std::string& filename ) {
                this->filename = filename;
                bool result = VelodyneCapture::open(filename);
                result &= VLP16Capture::isOpen();
                this->frameNumber = 0;
                if(result) this->moveToNext();

                return result;
            }
            bool restart() {
                if(this->isRun()) {
                    this->close();
                }
                if(filename.compare("") == 0) {
                    return false;
                }
                return this->open(filename);
            }

            void moveToNext() {
                if( this->mutex.try_lock() ){
                    if( !this->queue.empty() ){
                        this->lasers = std::move(this->queue.front());
                        this->queue.pop();
                    }
                    this->mutex.unlock();
                }
                while(this->isRun())
                {
                    if(this->queue.size() > 0) break;
                }
                this->frameNumber++;
            }

            void moveToNext(const int n) {
                for(int i = 0; i < n; i++) {
                    this->moveToNext();
                }
            }

            void setOffset(const double &azimuth, const double &x, const double &y, const double &z) {
                this->offsetAzimuth = azimuth;
                this->offsetX = x;
                this->offsetY = y;
                this->offsetZ = z;
            }

            void setTransformMatrix(Eigen::Matrix4f &transformMatrix) {
                this->transformMatrix = transformMatrix;
                this->transform = true;
            }

            void operator >> (std::vector<Laser>& lasers) {
                lasers = this->lasers;
            }

            void operator >> (boost::shared_ptr<pcl::PointCloud<pcl::PointXYZ>> &cloud) {
                pcl::PointXYZ point;
                Eigen::Vector4f p;

                if(!cloud ) cloud.reset(new pcl::PointCloud<pcl::PointXYZ>());

                for( const velodyne::Laser laser : this->lasers ){
                    const double distance = static_cast<double>( laser.distance );
                    const double azimuth  = laser.azimuth  * M_PI / 180.0;
                    const double vertical = laser.vertical * M_PI / 180.0;
                
                    point.x = static_cast<float>( ( distance * std::cos( vertical ) ) * std::sin( azimuth ) );
                    point.y = static_cast<float>( ( distance * std::cos( vertical ) ) * std::cos( azimuth ) );
                    point.z = static_cast<float>( ( distance * std::sin( vertical ) ) );
                    p(0) = static_cast<float>( ( distance * std::cos( vertical ) ) * std::sin( azimuth ) );
                    p(1) = static_cast<float>( ( distance * std::cos( vertical ) ) * std::cos( azimuth ) );
                    p(2) = static_cast<float>( ( distance * std::sin( vertical ) ) );
                    p(3) = 1;
                
                    if( point.x == 0.0f && point.y == 0.0f && point.z == 0.0f ){
                        continue;
                    }

                    if(std::abs(offsetAzimuth) > EPSILON) {
                        double x = point.x*std::cos(this->offsetAzimuth) + point.y*std::sin(this->offsetAzimuth);
                        double y = point.x*(-std::sin(this->offsetAzimuth)) + point.y*std::cos(this->offsetAzimuth);
                        double z = point.z;

                        point.x = x;
                        point.y = y;
                        point.z = z;
                    }
                    point.x = point.x + this->offsetX;
                    point.y = point.y + this->offsetY;
                    point.z = point.z + this->offsetZ;

                    cloud->points.push_back(point);
                }
                cloud->width = static_cast<uint32_t>(cloud->points.size());
                cloud->height = 1;
            }
    };
}

#endif
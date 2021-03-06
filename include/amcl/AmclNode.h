#ifndef AMCLNODE_H
#define AMCLNODE_H
#include "mcl/MCL.h"

class AmclNode : public MCL<AmclNode> 
{
  friend class MCL;
  public:
    AmclNode();
    ~AmclNode();

  protected:
    void laserReceived(const sensor_msgs::LaserScanConstPtr& laser_scan);
    void GLCB(){};
    void AIP(){};
    void RCCB();
};

#endif//AMCLNODE_H

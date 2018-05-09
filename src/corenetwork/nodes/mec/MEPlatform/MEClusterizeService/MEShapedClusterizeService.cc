//
//                           SimuLTE
//
// This file is part of a software released under the license included in file
// "license.pdf". This license can be also found at http://www.ltesimulator.com/
// The above file and the present reference are part of the software itself,
// and cannot be removed from it.
//

//
//  @author Angelo Buono
//


#include "MEShapedClusterizeService.h"
#include <cmath>
#include <algorithm>

Define_Module(MEShapedClusterizeService);

MEShapedClusterizeService::MEShapedClusterizeService() {

}

MEShapedClusterizeService::~MEShapedClusterizeService() {
}

void MEShapedClusterizeService::initialize(int stage)
{
    EV << "MEShapedClusterizeService::initialize - stage " << stage << endl;

    MEClusterizeService::initialize(stage);

    // avoid multiple initializations
    if (stage!=inet::INITSTAGE_APPLICATION_LAYER)
        return;

    proximityThreshold = par("proximityThreshold").doubleValue();                               //meter
    directionDelimiterThreshold = par("directionDelimiterThreshold").doubleValue();                //radiant
    roadLaneSize = par("roadLaneSize").doubleValue();                                           //meter
    triangleAngle = par("triangleAngle").doubleValue();                                             //radiant

    EV << "MEShapedClusterizeService::initialize - proximityThreshold: " << proximityThreshold << " - directionDelimiterThreshold: " << directionDelimiterThreshold << endl;
}
/*
 * #########################################################################################################################################
 */

void MEShapedClusterizeService::compute(){

    resetCarFlagsAndControls();
    resetClusters();

    computePlatoon("rectangle");
    //computePlatoon("triangle");
}

/*
 * #########################################################################################################################################
 */

void MEShapedClusterizeService::computePlatoon(std::string shape){

    EV << "MEClusterizeService::computePlatoon - starting clustering algorithm" << endl;

    std::map<int, std::vector<int>> adiacences;
    // Build Adiacences
    if(!shape.compare("triangle")){
        EV << "MEClusterizeService::computePlatoon - computing Triangle Adiacences..." << endl;
        computeTriangleAdiacences(adiacences);
    }
    else if(!shape.compare("rectangle")){
        EV << "MEClusterizeService::computePlatoon - computing Rectangle Adiacences..." << endl;
        computeRectangleAdiacences(adiacences);
    }

    // Select Best Follower
    EV << "MEClusterizeService::computePlatoon - selecting Followers..." << endl;
    selectFollowers(adiacences);

    // Scan the v2vConfig to update the platoonList filed!
    EV << "MEClusterizeService::computePlatoon - Updating Clusters...";
    updateClusters();


    //TESTING PRINT                                                                                             //TESTING
    EV << "\nMEClusterizeService::computePlatoon - FOLLOWERS:\n\n";
    std::map<int, car>::iterator it;
    for(it = cars.begin(); it != cars.end(); it++){

          EV << it->second.simbolicAddress << "\tfollowed by:\t" << it->second.follower << "\t[following:\t" << it->second.following << "]\n";
    }

    EV << "\nMEClusterizeService::computePlatoon - CLUSTERS:\n\n";

    std::map<int, cluster>::iterator cit;

    for(cit = clusters.begin(); cit != clusters.end(); cit++){

            EV << "Cluster #" << cit->second.id << ": " << cit->second.membersList << "\n";

            EV << "cluster members keys: ";
            for(std::vector<int>::iterator i = cit->second.members.begin(); i != cit->second.members.end(); i++)
                EV << *i << " ";
            EV << endl << endl;
    }
}

void MEShapedClusterizeService::computeTriangleAdiacences(std::map<int, std::vector<int>> &adiacences){

    std::map<int, car>::iterator it, it2;
    for(it = cars.begin(); it != cars.end(); it++){

        inet::Coord A(it->second.position);
        double a = it->second.angularPosition.alpha;
        inet::Coord B( A.x + proximityThreshold*cos(PI+a-triangleAngle) , A.y + proximityThreshold*sin(PI+a-triangleAngle) );
        inet::Coord C( A.x + proximityThreshold*cos(PI+a+triangleAngle) , A.y + proximityThreshold*sin(PI+a+triangleAngle) );

        for(it2 = cars.begin(); it2 != cars.end(); it2++){

            //cars going in the same direction
            if( (it != it2) && (abs(it->second.angularPosition.alpha - it2->second.angularPosition.alpha) <= directionDelimiterThreshold)){

            inet::Coord P(it2->second.position);

            //adding to the vector of adiacences of car it
            if(isInTriangle(P,A,B,C))
                adiacences[it->first].push_back(it2->first);
            }
        }
    }
}

void MEShapedClusterizeService::computeRectangleAdiacences(std::map<int, std::vector<int>> &adiacences){

    std::map<int, car>::iterator it, it2;
    for(it = cars.begin(); it != cars.end(); it++){

        double a = it->second.angularPosition.alpha;
        inet::Coord pos(it->second.position);
        inet::Coord A( pos.x + (roadLaneSize/2)*cos(a+PI/2), pos.y + (roadLaneSize/2)*sin(a+PI/2) );        // left-right side
        inet::Coord B( pos.x + (roadLaneSize/2)*cos(a-PI/2), pos.y + (roadLaneSize/2)*sin(a-PI/2) );        // left-right side
        inet::Coord C( B.x + proximityThreshold*cos(PI+a), B.y + proximityThreshold*sin(PI+a) );            // behind side
        inet::Coord D( A.x + proximityThreshold*cos(PI+a), A.y + proximityThreshold*sin(PI+a) );            // behind side

        for(it2 = cars.begin(); it2 != cars.end(); it2++){

            //cars going in the same direction
            if( (it != it2) && (abs(it->second.angularPosition.alpha - it2->second.angularPosition.alpha) <= directionDelimiterThreshold)){

            inet::Coord P(it2->second.position);

            //adding to the vector of adiacences of car it
            if(isInRectangle(P, A, B, C, D))
                adiacences[it->first].push_back(it2->first);
            }
        }
    }
}

void MEShapedClusterizeService::selectFollowers(std::map<int, std::vector<int>> &adiacences){

    std::map<int, std::vector<int>>::iterator it;
    bool found;
    for(it = adiacences.begin(); it != adiacences.end(); it++){

        found = false;
        std::vector<int>::iterator it2 = it->second.begin();
        int follower = *it2;

        for( it2; it2 != it->second.end(); it2++){

            if(!cars[*it2].isFollower){
                // update follower if it2 car (v2vInfo[*it2] entry) has distance lesser than the follower one!
                if(cars[it->first].position.distance(cars[*it2].position) <= cars[it->first].position.distance(cars[follower].position)){
                    follower = *it2;
                    found = true;
                }
            }
        }
        if(found){
            cars[it->first].follower = cars[follower].simbolicAddress;
            cars[it->first].followerKey = follower;

            cars[follower].following = cars[it->first].simbolicAddress;
            cars[follower].followingKey = it->first;
            cars[follower].isFollower = true;
        }
    }
}

void MEShapedClusterizeService::updateClusters(){

    int colorSize = colors.size();
    // Scan the v2vConfig to update the platoonList filed!
    std::map<int, car>::iterator it;
    for(it = cars.begin(); it != cars.end(); it++){

        if(it->second.followingKey == -1){

            std::stringstream platoonList;
            int k = it->first;

            // update clusters
            while(k != -1){
                clusters[(it->second).id].members.push_back(k);
                cars[k].clusterID = (it->second).id;
                                                                                                    //TODO
                                                                                                    //  adding the txMode computation!
                platoonList << cars[k].simbolicAddress << " -> ";
                k = cars[k].followerKey;
            }
            clusters[(it->second).id].membersList = platoonList.str();
            clusters[(it->second).id].id = (it->second).id;
            clusters[(it->second).id].color = colors.at( ( it->first ) % colorSize);
        }
    }
}

bool MEShapedClusterizeService::isInTriangle(inet::Coord P, inet::Coord A, inet::Coord B, inet::Coord C){

      //considering all points relative to A
      inet::Coord v0 = B-A;   // B w.r.t A
      inet::Coord v1 = C-A;   // C w.r.t A
      inet::Coord v2 = P-A;   // P w.r.t A

      // writing v2 = u*v0 + v*v1 (linear combination in the Plane defined by vectors v0 and v1)
      // and finding u and v (scalar)

      double den = ((v0*v0) * (v1*v1)) - ((v0*v1) * (v1*v0));

      double u = ( ((v1*v1) * (v2*v0)) - ((v1*v0) * (v2*v1)) ) / den;
      double v = ( ((v0*v0) * (v2*v1)) - ((v0*v1) * (v2*v0)) ) / den;

      // checking if coefficientes u and v are constrained in [0-1], that means inside the triangle ABC
      if(u>=0 && v>=0 && u+v<=1)
      return true;
      else
      return false;
}

bool MEShapedClusterizeService::isInRectangle(inet::Coord P, inet::Coord A, inet::Coord B, inet::Coord C, inet::Coord D)
{
      return isInTriangle(P, A, B, C) || isInTriangle(P, D, B, C);
}

void MEShapedClusterizeService::resetCarFlagsAndControls(){

    // Scan the cars to update the flags and control fields!
    std::map<int, car>::iterator it;
    for(it = cars.begin(); it != cars.end(); it++){

        it->second.followingKey = -1;
        it->second.followerKey = -1;
        it->second.isFollower = false;

        it->second.following = "";
        it->second.follower = "";
    }
}

void MEShapedClusterizeService::resetClusters(){

    clusters.clear();
}
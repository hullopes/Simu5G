//
//                  Simu5G
//
// Authors: Giovanni Nardini, Giovanni Stea, Antonio Virdis (University of Pisa)
//
// This file is part of a software released under the license included in file
// "license.pdf". Please read LICENSE and README files before using it.
// The above files and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _BINDER_H_
#define _BINDER_H_

#include <omnetpp.h>
#include <string>

#include <inet/networklayer/contract/ipv4/Ipv4Address.h>
#include <inet/networklayer/common/L3Address.h>
#include "common/LteCommon.h"
#include "common/blerCurves/PhyPisaData.h"
#include "nodes/ExtCell.h"
#include "stack/mac/layer/LteMacBase.h"
#include "stack/backgroundTrafficGenerator/generators/TrafficGeneratorBase.h"
#include <boost/bind.hpp>

class UeStatsCollector;

//!VH
#include "stack/mac/allocator/LteAllocatorUtils.h"
#include "stack/mac/allocator/LteAllocationModule.h"

#include <iostream>
#include <string>
#include <unordered_map>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/error/en.h"

using namespace rapidjson;

/// forward declarations
//class LteAllocationModule;

/**
 * The Binder module has one instance in the whole network.
 * It stores global mapping tables with OMNeT++ module IDs,
 * IP addresses, etc.
 */

class Binder : public omnetpp::cSimpleModule
{
  private:
    typedef std::map<MacNodeId, std::map<MacNodeId, bool> > DeployedUesMap;

    std::map<inet::Ipv4Address, MacNodeId> macNodeIdToIPAddress_;
    std::map<inet::Ipv4Address, MacNodeId> nrMacNodeIdToIPAddress_;
    std::map<MacNodeId, char*> macNodeIdToModuleName_;
    std::map<MacNodeId, cModule*> macNodeIdToModuleRef_;
    std::map<MacNodeId, LteMacBase*> macNodeIdToModule_;
    std::vector<MacNodeId> nextHop_; // MacNodeIdMaster --> MacNodeIdSlave
    std::vector<MacNodeId> secondaryNodeToMasterNode_;
    std::map<int, OmnetId> nodeIds_;

    // stores the IP address of the MEC hosts in the simulation
    std::set<inet::L3Address> mecHostAddress_;

    // for GTP tunneling with MEC hosts involved
    // this map associates the L3 address of the Virt. Infrastructure of a MEC host to
    // the IP address of the corresponding UPF
    std::map<inet::L3Address, inet::L3Address> mecHostToUpfAddress_;

    // list of static external cells. Used for intercell interference evaluation
    std::map<double, ExtCellList> extCellList_;

    // list of static external cells. Used for intercell interference evaluation
    std::map<double, BackgroundSchedulerList> bgSchedulerList_;

    // list of all eNBs. Used for inter-cell interference evaluation
    std::vector<EnbInfo*> enbList_;
    //!VH controller for downgraded BSs
    std::vector<bool> downgradedByUpperCN;

    // list of all UEs. Used for inter-cell interference evaluation
    std::vector<UeInfo*> ueList_;

    // list of all background traffic manager. Used for background UEs CQI computation
    std::vector<BgTrafficManagerInfo*> bgTrafficManagerList_;

    typedef std::map<unsigned int, std::map<unsigned int, double> > BgInterferenceMatrix;
    // map of maps storing the mutual interference between BG cells
    BgInterferenceMatrix bgCellsInterferenceMatrix_;
    // map of maps storing the mutual interference between BG UEs
    BgInterferenceMatrix bgUesInterferenceMatrix_;
    // maximum data rate achievable in one RB (NED parameter)
    double maxDataRatePerRb_;

    MacNodeId macNodeIdCounter_[3]; // MacNodeId Counter
    DeployedUesMap dMap_; // DeployedUes --> Master Mapping

    //!VH define the enb overflow counter
    std::vector<double> enbListOverflow_;


    /*
     * Carrier Aggregation support
     */
    // total number of logical bands in the system. These bands
    unsigned int totalBands_;
    CarrierInfoMap componentCarriers_;

    // for each carrier, store the UEs that are able to use it
    typedef std::map<double, UeSet > CarrierUeMap;
    CarrierUeMap carrierUeMap_;


    // hack to link carrier freq to numerology index
    std::map<double, NumerologyIndex> carrierFreqToNumerologyIndex_;
    // max numerology index used by UEs
    std::vector<NumerologyIndex> ueMaxNumerologyIndex_;
    // set of numerologies used by each UE
    std::map<MacNodeId, std::set<NumerologyIndex> > ueNumerologyIndex_;

    /*
     * Uplink interference support
     */
    typedef std::map<double, std::vector< std::vector< std::vector<UeAllocationInfo> > > > UplinkTransmissionMap;
    // for each carrier frequency, for both previous and current TTIs, for each RB, stores the UE (nodeId and ref to the PHY module) that transmitted/are transmitting within that RB
    UplinkTransmissionMap ulTransmissionMap_;
    // TTI of the last update of the UL band status
    omnetpp::simtime_t lastUpdateUplinkTransmissionInfo_;
    // TTI of the last UL transmission (used for optimization purposes, see initAndResetUlTransmissionInfo() )
    omnetpp::simtime_t lastUplinkTransmission_;

    /*
     * X2 Support
     */
    typedef std::map<X2NodeId, std::list<int> > X2ListeningPortMap;
    X2ListeningPortMap x2ListeningPorts_;

    std::map<MacNodeId, std::map<MacNodeId, inet::L3Address> > x2PeerAddress_;

//    std::map<MacNodeId, L3Address> x2Address_;

    /*
     * D2D Support
     */
    // determines if two D2D-capable UEs are communicating in D2D mode or Infrastructure Mode
    std::map<MacNodeId, std::map<MacNodeId, LteD2DMode> > d2dPeeringMap_;

    /*
     * Multicast support
     */
    // register here the IDs of the multicast group where UEs participate
    typedef std::set<uint32_t> MulticastGroupIdSet;
    std::map<MacNodeId, MulticastGroupIdSet> multicastGroupMap_;
    std::set<MacNodeId> multicastTransmitterSet_;

    /*
     * Handover support
     */
    // store the id of the UEs that are performing handover
    std::set<MacNodeId> ueHandoverTriggered_;
    std::map<MacNodeId, std::pair<MacNodeId, MacNodeId> > handoverTriggered_;
  protected:
    virtual void initialize(int stages) override;
    virtual int numInitStages() const override { return inet::NUM_INIT_STAGES; }
    virtual void handleMessage(omnetpp::cMessage *msg) override
    {
    }
    virtual void finish() override;

  public:
    Binder()
    {
        macNodeIdCounter_[0] = ENB_MIN_ID;
        macNodeIdCounter_[1] = UE_MIN_ID;
        macNodeIdCounter_[2] = NR_UE_MIN_ID;

        totalBands_ = 0;
        lastUpdateUplinkTransmissionInfo_ = 0.0;
        lastUplinkTransmission_ = 0.0;
    }

    unsigned int getTotalBands()
    {
        return totalBands_;
    }

    virtual ~Binder()
    {
        while(enbList_.size() > 0){
            delete enbList_.back();
            enbList_.pop_back();
        }

        while(bgTrafficManagerList_.size() > 0){
            delete bgTrafficManagerList_.back();
            bgTrafficManagerList_.pop_back();
        }

        for (auto it = macNodeIdToModuleName_.begin(); it != macNodeIdToModuleName_.end(); ++it)
            delete[] it->second;

        for (auto it = ueList_.begin(); it != ueList_.end(); ++it)
            delete (*it);
        ueList_.clear();
    }

    /**
     * Registers a carrier to the global Binder module
     */
    void registerCarrier(double carrierFrequency, unsigned int carrierNumBands, unsigned int numerologyIndex,
            bool useTdd=false, unsigned int tddNumSymbolsDl=0, unsigned int tddNumSymbolsUl=0);

    /**
     * Registers a UE to a given carrier
     */
    void registerCarrierUe(double carrierFrequency, unsigned int numerologyIndex, MacNodeId ueId);

    /**
     * Returns the set of UEs enabled on the given carrier
     */
    const UeSet& getCarrierUeSet(double carrierFrequency);

    /**
     * Returns the max numerology index used by the given UE
     */
    NumerologyIndex getUeMaxNumerologyIndex(MacNodeId ueId);

    /**
     * Returns the numerology indices used by the given UE
     */
    const std::set<NumerologyIndex>* getUeNumerologyIndex(MacNodeId ueId);

    /**
     * Returns the numerology associated to a carrier frequency
     */
    NumerologyIndex getNumerologyIndexFromCarrierFreq(double carrierFreq) { return carrierFreqToNumerologyIndex_[carrierFreq]; }

    /**
     * Returns the slot duration associated to the numerology (in seconds)
     */
    double getSlotDurationFromNumerologyIndex(NumerologyIndex numerologyIndex) { return pow(2.0, (-1) * (int)numerologyIndex) / 1000.0; }

    /**
     * Compute slot format object given the number of DL and UL symbols
     */
    SlotFormat computeSlotFormat(bool useTdd, unsigned int tddNumSymbolsDl, unsigned int tddNumSymbolsUl);

    /**
     * Returns the slot format for the given carrier
     */
    SlotFormat getSlotFormat(double carrierFrequency);

    /**
     * Registers a node to the global Binder module.
     *
     * The binder assigns an IP address to the node, from which it is derived
     * an unique macNodeId.
     * The node registers its moduleId (omnet ID), and if it's a UE,
     * it registers also the association with its master node.
     *
     * @param module pointer to the module to be registered
     * @param type type of this node (ENODEB, GNODEB, UE)
     * @param masterId id of the master of this node, 0 if none (node is an eNB)
     * @return macNodeId assigned to the module
     */
    MacNodeId registerNode(cModule *module, RanNodeType type, MacNodeId masterId = 0, bool registerNr = false);

    /**
     * Un-registers a node from the global Binder module.
     */
    void unregisterNode(MacNodeId id);

    /**
     * registerNextHop() is called by the IP2NIC module at network startup
     * to bind each slave with its masters. It is also
     * called on handovers to synchronize the nextHop table:
     *
     * It registers a slave to its current master
     *
     * @param masterId MacNodeId of the Master
     * @param slaveId MacNodeId of the Slave
     */
    void registerNextHop(MacNodeId masterId, MacNodeId slaveId);

    /**
     * registerNextHop() is called on handovers to sychronize
     * the nextHop table:
     *
     * It unregisters the slave from its old master
     *
     * @param masterId MacNodeId of the Master
     * @param slaveId MacNodeId of the Slave
     */
    void unregisterNextHop(MacNodeId masterId, MacNodeId slaveId);

    /**
     * registerMasterNode()
     *
     * It registers the secondary node to its master node (used for dual connectivity)
     *
     * @param masterId MacNodeId of the Master
     * @param slaveId MacNodeId of the Slave
     */
    void registerMasterNode(MacNodeId masterId, MacNodeId slaveId);

    /**
     * getOmnetId() returns the Omnet Id of the module
     * given its MacNodeId
     *
     * @param nodeId MacNodeId of the module
     * @return OmnetId of the module
     */
    OmnetId getOmnetId(MacNodeId nodeId);

    /*
     * get iterators for the list of nodes
     */
    std::map<int, OmnetId>::const_iterator getNodeIdListBegin();
    std::map<int, OmnetId>::const_iterator getNodeIdListEnd();

    /**
     * getMacNodeIdFromOmnetId() returns the MacNodeId of the module
     * given its OmnetId
     *
     * @param id OmnetId of the module
     * @return MacNodeId of the module
     */
    MacNodeId getMacNodeIdFromOmnetId(OmnetId id);

    /*
     * getMacFromMacNodeId() returns the reference to the LteMacBase module
     * given the MacNodeId of a node
     *
     * @param id MacNodeId of the module
     * @return LteMacBase* of the module
     */
    LteMacBase* getMacFromMacNodeId(MacNodeId id);

    /**
     * getNextHop() returns the master of
     * a given slave
     *
     * @param slaveId MacNodeId of the Slave
     * @return MacNodeId of the master
     */
    MacNodeId getNextHop(MacNodeId slaveId);

    /**
     * getMasterNode() returns the master of
     * a given slave (used for dual connectivity)
     *
     * @param slaveId MacNodeId of the Slave
     * @return MacNodeId of the master
     */
    MacNodeId getMasterNode(MacNodeId slaveId);

    /**
     * Returns the MacNodeId for the given IP address
     *
     * @param address IP address
     * @return MacNodeId corresponding to the IP address
     */
    MacNodeId getMacNodeId(inet::Ipv4Address address)
    {
        if (macNodeIdToIPAddress_.find(address) == macNodeIdToIPAddress_.end())
            return 0;
        MacNodeId nodeId = macNodeIdToIPAddress_[address];

        // if the UE is disconnected (its master node is 0), check the NR node Id
        if (getNextHop(nodeId) == 0)
            return getNrMacNodeId(address);
        return nodeId;
    }
    /**
     * Returns the MacNodeId for the given IP address
     *
     * @param address IP address
     * @return MacNodeId corresponding to the IP address
     */
    MacNodeId getNrMacNodeId(inet::Ipv4Address address)
    {
        if (nrMacNodeIdToIPAddress_.find(address) == nrMacNodeIdToIPAddress_.end())
            return 0;
        return nrMacNodeIdToIPAddress_[address];
    }

    /**
     * author Alessandro Noferi
     *
     * Returns the IP address for the given MacNodeId
     *
     * @param MacNodeId of the node
     * @return IP address corresponding to the MacNodeId
     *
     */
    inet::Ipv4Address getIPv4Address(MacNodeId nodeId)
    {
        for (const auto& kv : macNodeIdToIPAddress_) {
            if(kv.second == nodeId)
                return kv.first;
        }
        for (const auto& kv : nrMacNodeIdToIPAddress_) {
            if(kv.second == nodeId)
                return kv.first;
        }
        return inet::Ipv4Address::UNSPECIFIED_ADDRESS;
    }
    /**
     * Returns the X2NodeId for the given IP address
     *
     * @param address IP address
     * @return X2NodeId corresponding to the IP address
     */
    X2NodeId getX2NodeId(inet::Ipv4Address address)
    {
        return getMacNodeId(address);
    }
    /**
     * Associates the given IP address with the given MacNodeId.
     *
     * @param address IP address
     */
    void setMacNodeId(inet::Ipv4Address address, MacNodeId nodeId)
    {
        if (isNrUe(nodeId))
            nrMacNodeIdToIPAddress_[address] = nodeId;
        else
            macNodeIdToIPAddress_[address] = nodeId;
    }
    /**
     * Associates the given IP address with the given X2NodeId.
     *
     * @param address IP address
     */
    void setX2NodeId(inet::Ipv4Address address, X2NodeId nodeId)
    {
        setMacNodeId(address, nodeId);
    }
    inet::L3Address getX2PeerAddress(X2NodeId srcId, X2NodeId destId)
    {
        return x2PeerAddress_[srcId][destId];
    }
    void setX2PeerAddress(X2NodeId srcId, X2NodeId destId, inet::L3Address interfAddr)
    {
        std::pair<X2NodeId, inet::L3Address> p(destId, interfAddr);
        x2PeerAddress_[srcId].insert(p);
    }
//    L3Address getX2Address(X2NodeId nodeId)
//    {
//        return x2Address_[nodeId];
//    }
//    void setX2Address(X2NodeId nodeId, L3Address interfAddr)
//    {
//        if (x2Address_.find(nodeId) != x2Address_.end())
//            return;
//        x2Address_[nodeId] = interfAddr;
//    }

    /**
     * Register the address of MEC Hosts in the simulation
     */
    void registerMecHost(const inet::L3Address& mecHostAddress);
    /**
     * Associates the given MEC Host address to its corresponding UPF
     */
    void registerMecHostUpfAddress(const inet::L3Address& mecHostAddress, const inet::L3Address& gtpAddress);
    /**
     * Returns true if the given address belongs to a MEC host
     */
    bool isMecHost(const inet::L3Address& mecHostAddress);
    /**
     * Returns the UPF address corresponding to the given MEC Host address
     */
    const inet::L3Address& getUpfFromMecHost(const inet::L3Address& mecHostAddress);
    /**
     * Associates the given MAC node ID to the name of the module
     */
    void registerName(MacNodeId nodeId, const char* moduleName);
    /**
     * Returns the module name for the given MAC node ID
     */
    const char* getModuleNameByMacNodeId(MacNodeId nodeId);
    /**
     * Associates the given MAC node ID to the module
     */
    void registerModule(MacNodeId nodeId, cModule* module);
    /**
     * Returns the module for the given MAC node ID
     */
    cModule* getModuleByMacNodeId(MacNodeId nodeId);

    /*
     * getDeployedUes() returns the affiliates
     * of a given eNodeB
     */
    ConnectedUesMap getDeployedUes(MacNodeId localId, Direction dir);
    PhyPisaData phyPisaData;

    int getNodeCount(){
        return nodeIds_.size();
    }

    int addExtCell(ExtCell* extCell, double carrierFrequency)
    {
        if (extCellList_.find(carrierFrequency) == extCellList_.end())
            extCellList_[carrierFrequency] = ExtCellList();

        extCellList_[carrierFrequency].push_back(extCell);
        return extCellList_[carrierFrequency].size() - 1;
    }

    ExtCellList getExtCellList(double carrierFrequency)
    {
        return extCellList_[carrierFrequency];
    }

    int addBackgroundScheduler(BackgroundScheduler* bgScheduler, double carrierFrequency)
    {
        if (bgSchedulerList_.find(carrierFrequency) == bgSchedulerList_.end())
            bgSchedulerList_[carrierFrequency] = BackgroundSchedulerList();

        bgSchedulerList_[carrierFrequency].push_back(bgScheduler);
        return bgSchedulerList_[carrierFrequency].size() - 1;
    }

    BackgroundSchedulerList* getBackgroundSchedulerList(double carrierFrequency)
    {
        return &bgSchedulerList_[carrierFrequency];
    }

    void addEnbInfo(EnbInfo* info)
    {
        enbList_.push_back(info);
        //!VH init enbListOverflow_ vector
        enbListOverflow_.push_back(0);
        //!VH setting up the default value, i.e., this BS was never downgraded
        downgradedByUpperCN.push_back(false);
    }

    std::vector<EnbInfo*> * getEnbList()
    {
        return &enbList_;
    }

    void addUeInfo(UeInfo* info)
    {
        ueList_.push_back(info);
    }

    std::vector<UeInfo*> * getUeList()
    {
        return &ueList_;
    }

    void addBgTrafficManagerInfo(BgTrafficManagerInfo* info)
    {
        bgTrafficManagerList_.push_back(info);
    }

    std::vector<BgTrafficManagerInfo*> * getBgTrafficManagerList()
    {
        return &bgTrafficManagerList_;
    }

    Cqi meanCqi(std::vector<Cqi> bandCqi,MacNodeId id,Direction dir);

    Cqi medianCqi(std::vector<Cqi> bandCqi,MacNodeId id,Direction dir);

    /*
     * Uplink interference support
     */
    omnetpp::simtime_t getLastUpdateUlTransmissionInfo();
    void initAndResetUlTransmissionInfo();
    void storeUlTransmissionMap(double carrierFreq, Remote antenna, RbMap& rbMap, MacNodeId nodeId, MacCellId cellId, LtePhyBase* phy, Direction dir);
    void storeUlTransmissionMap(double carrierFreq, Remote antenna, RbMap& rbMap, MacNodeId nodeId, MacCellId cellId, TrafficGeneratorBase* trafficGen, Direction dir);  // overloaded function for bgUes
    const std::vector<std::vector<UeAllocationInfo> >* getUlTransmissionMap(double carrierFreq, UlTransmissionMapTTI t);
    /*
     * X2 Support
     */
    void registerX2Port(X2NodeId nodeId, int port);
    int getX2Port(X2NodeId nodeId);

    /*
     * D2D Support
     */
    bool checkD2DCapability(MacNodeId src, MacNodeId dst);
    bool getD2DCapability(MacNodeId src, MacNodeId dst);

    std::map<MacNodeId, std::map<MacNodeId, LteD2DMode> >* getD2DPeeringMap();
    void setD2DMode(MacNodeId src, MacNodeId dst, LteD2DMode mode);
    LteD2DMode getD2DMode(MacNodeId src, MacNodeId dst);
    bool isFrequencyReuseEnabled(MacNodeId nodeId);
    /*
     * Multicast Support
     */
    // add the group to the set of multicast group of nodeId
    void registerMulticastGroup(MacNodeId nodeId, int32_t groupId);
    // check if the node is enrolled in the group
    bool isInMulticastGroup(MacNodeId nodeId, int32_t groupId);
    // add one multicast transmitter
    void addD2DMulticastTransmitter(MacNodeId nodeId);
    // get multicast transmitters
    std::set<MacNodeId>& getD2DMulticastTransmitters();

    /*
     *  Handover support
     */
    void addUeHandoverTriggered(MacNodeId nodeId);
    bool hasUeHandoverTriggered(MacNodeId nodeId);
    void removeUeHandoverTriggered(MacNodeId nodeId);

    void addHandoverTriggered(MacNodeId nodeId, MacNodeId srcId, MacNodeId destId);
    const std::pair<MacNodeId, MacNodeId>* getHandoverTriggered(MacNodeId nodeId);
    void removeHandoverTriggered(MacNodeId nodeId);

    void updateUeInfoCellId(MacNodeId nodeId, MacCellId cellId);
    MacNodeId findUeInfoCellId(MacNodeId id); //!VH function to search a specific UE

    /*
     *  Background UEs and cells Support
     */
    void computeAverageCqiForBackgroundUes();
    void updateMutualInterference(unsigned int bgTrafficManagerId, unsigned int numBands, Direction dir);
    double computeInterferencePercentageDl(double n, double k, unsigned int numBands);
    double computeInterferencePercentageUl(double n, double k, double nTotal, double kTotal);
    double computeSinr(unsigned int bgTrafficManagerId, int bgUeId, double txPower, inet::Coord txPos, inet::Coord rxPos, Direction dir, bool losStatus);
    double computeRequestedRbsFromSinr(double sinr, double reqLoad);

    /*
     * @author Alessandro Noferi.
     *
     * UeStatsCollector management
     */

    /* this method adds the UeStastCollector reference to the baseStationStatsCollector
     * structure.
     * @params:
     *  ue: MacNodeId of the ue
     *  ueCollector: reference to the collector
     *  cell: MacCellId of the target eNB
     */
    void addUeCollectorToEnodeB(MacNodeId ue, UeStatsCollector* ueCollector, MacCellId cell);

    /* this method moves the UeStastCollector reference between the eNB/gNB's baseStationStatsCollector
     * structure.
     * @params:
     *  ue: MacNodeId of the ue
     *  oldCell: MacCellId of the source eNB
     *  newCell: MacCellId of the target eNB
     */
    void moveUeCollector(MacNodeId ue, MacCellId oldCell, MacCellId newCell);

    RanNodeType getBaseStationTypeById(MacNodeId);


    //VH dynamic configuration
    /// the running TxMode, number of layers and RI
    int currentTxModeId_;
    std::vector<int> currentLayers_;
    std::vector<int> currentMaxRI_;
    int currentMaxModOrder_; //!VH max allowed modulation order
    int currentMaxRBs_; // !VH max allowed resource blocks / BW
    int currentMaxCQI_; //!VH max allowed CQI. In fact, it can be the same as the maximum modulation order, but it makes the process easier.
    std::vector<int> currentMaxCQI;
    std::vector<int> reductionPolicy_;
    std::vector<int> CNProcCapacity;
    std::vector<int> originalCurrentLayers_;
    std::vector<int> originalCurrentMaxRI_;
    int placementSolutionDelay_; //!VH time to receive a solution from the solver, in TTIs
    int placementSolultionDelayCounter; //counter/timer
    int periodicMonitoring_; //!VH time to employ a periodic monitoring
    int periodicMonitoringCounter_; //!VH counting the elapsed TTIs until the periodic monitoring can be employed
    int timer;
    //std::map<MacNodeId, double> placementSolution;
    std::unordered_map<MacNodeId, std::unordered_map<MacNodeId, double>> placementSolution;
    std::unordered_map<MacNodeId, std::unordered_map<MacNodeId, double>> placementSolutionA;
    //std::unordered_map<int, std::unordered_map<int, double>> placementSolution;

    void setCurrentTxModeId(int txModeId)
    {
        currentTxModeId_ = txModeId;
    }

    void setCurrentLayers(int numLayers)
    {
        currentLayers_.push_back(numLayers);
        originalCurrentLayers_.push_back(numLayers);
    }
    void setCurrentMaxRI(int maxRI)
    {
        currentMaxRI_.push_back(maxRI);
        originalCurrentMaxRI_.push_back(maxRI);
    }
    void setCurrentMaxModOrder(int maxModOrder)
    {
        currentMaxModOrder_ = maxModOrder;
    }
    void setCurrentMaxRBs(int maxRBs)
    {
        currentMaxRBs_ = maxRBs;
    }
    void setCurrentMaxCQI(int maxCQI)
    {
        //currentMaxCQI_ = maxCQI;
        currentMaxCQI.push_back(maxCQI);
    }
    void updateCurrentMaxCQI(int id, int maxCQI){
        currentMaxCQI[id-1] = maxCQI;
    }
    void setCNProcCapacity(int procCapacity)
    {
        CNProcCapacity.push_back(procCapacity);
    }
    void setLastBSProcDemandProportion(MacNodeId nodeId, double costProp)
    {
        std::vector<EnbInfo*>::iterator it = enbList_.begin(), et = enbList_.end();
        for ( ; it != et; ++it)
        {
            EnbInfo* info = *it;
            if (info->id == nodeId){
                info->lastBSProcDemandProportion = costProp;
                break;
            }
        }
    }
    void setPlacementSolution_old(std::string pSolution)
    {
        std::stringstream ss(pSolution);
        MacNodeId key;
        double value;

        // Parsing the string
        char ch;
        ss >> ch >> key >> ch >> value >> ch;
        //placementSolution.insert({key, value});
    }
    void setPlacementSolution__(std::string pSolution)
    {
        using namespace rapidjson;
        // JSON string to be parsed
        //const std::string jsonStr = R"(pSolution)";
        const std::string jsonStr = R"(pSolution)";

            // Parse the JSON string using RapidJSON
            Document doc;
            doc.Parse(jsonStr.c_str());

            // Iterate through the parsed JSON object and populate the map
            for (auto& outerVal : doc.GetObject()) {
                int outerKey = std::stoi(outerVal.name.GetString());
                for (auto& innerVal : outerVal.value.GetObject()) {
                    int innerKey = std::stoi(innerVal.name.GetString());
                    double innerValue = innerVal.value.GetDouble();
                    placementSolution[outerKey][innerKey] = innerValue;
                }
            }
    }

    void setPlacementSolution(std::string pSolution) {
        rapidjson::Document document;
        rapidjson::ParseResult result = document.Parse(pSolution.c_str());

        if (!result) {
            std::cerr << "Error parsing JSON: " << rapidjson::GetParseError_En(result.Code()) << '\n';
            return;
        }

        if (!document.IsObject()) {
            std::cerr << "Error: JSON root is not an object\n";
            return;
        }

        for (auto& outer_entry : document.GetObject()) {
            if (!outer_entry.value.IsObject()) {
                std::cerr << "Error: outer key is not an object\n";
                continue;
            }

            int outer_key = std::stoi(outer_entry.name.GetString());

            for (auto& inner_entry : outer_entry.value.GetObject()) {
                if (!inner_entry.value.IsNumber()) {
                    std::cerr << "Error: inner value is not a number\n";
                    continue;
                }

                int inner_key = std::stoi(inner_entry.name.GetString());
                double inner_val = inner_entry.value.GetDouble();

                placementSolution[outer_key][inner_key] = inner_val;
            }
        }
    }

    void setPlacementSolutionDelay(int time){
        placementSolutionDelay_ = time;
    }

    void setReductionPolicy(int reductionPolicy) {
        reductionPolicy_.push_back(reductionPolicy);
    }

    void incPlacementSolutionDelayCounter(){
        placementSolultionDelayCounter += 1;
    }

    void resetPlacementSolutionDelayCounter(){
        placementSolultionDelayCounter = 0;
    }
    void setPeriodicMonitoring(int time){
        periodicMonitoring_ = time;
    }
    int getPeriodicMonitoring(){
        return periodicMonitoring_;
    }
    void incPeriodicMonitoringCounter(){
        periodicMonitoringCounter_ += 1;
    }
    void resetPeriodicMonitoringCounter(){
        periodicMonitoringCounter_ = 0;
    }
    void incTimer(){
        timer++;
    }
    void resetTimer(){
        timer = 0;
    }
    int getTimer(){
        return timer;
    }
    int getPeriodicMonitoringCounter(){
        return periodicMonitoringCounter_;
    }
    int getPlacementSolultionDelayCounter(){
        return placementSolultionDelayCounter;
    }

    double getPlacementSolutionDelay(){
        return placementSolutionDelay_;
    }

    int getReductionPolicy(MacNodeId id){
        return reductionPolicy_[id - 1];
    }

    int getCurrentTxModeId()
    {
        return currentTxModeId_;
    }

    int getCurrentLayers(MacNodeId id)
    {
        return currentLayers_[id - 1];
    }
    int getCurrentMaxRI(MacNodeId id)
    {
        return currentMaxRI_[id - 1];
    }
    int getCurrentMaxModOrder()
    {
        return currentMaxModOrder_;
    }
    int getCurrentMaxRBs()
    {
        return currentMaxRBs_;
    }
    int getCurrentMaxCQI(int id)
    {
        return currentMaxCQI[id-1];
    }
    int getCNProcCapacity(int id)
    {
        return CNProcCapacity[id-1];
    }
    int getLastEnbInfo()
    {
        if (enbList_.size() == 0)
            return 0;
        else
            return enbList_.size();
    }

    void setUECurrentProcessingDemand(MacNodeId id, double procDemand);
    //void computeAndRecordProcessingDemand(LteAllocationModule *allocator);
    double computeAndRecordProcessingDemand(LteAllocationModule *allocator, MacNodeId nodeId);
    double computeBSProcessingDemand(LteAllocationModule *allocator);
    std::tuple<double, double> computeClusterProcessingDemand(LteAllocationModule *allocator, MacNodeId nodeId);
    void applyPolicy(MacNodeId nodeId, double clusterCost);
    int changeCurrentMaxCQI(MacNodeId nodeId, double clusterCost, double CNProcCapacity);
    void proccessPeriodicMonitoring();
    void changeScenario();
};

#endif

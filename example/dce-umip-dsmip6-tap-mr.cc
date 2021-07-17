/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

//
// NEMO (Network Mobility) simulation with umip (mip6d) and net-next-2.6.
//
// UMIP: http://www.umip.org/git/umip.git
// patchset: 0daa3924177f326e26ed8bbb9dc9f0cdf8a51618
// build:  CFLAGS="-fPIC -g" CXXFLAGS="-fPIC -g" LDFLAGS="-pie -g" ./configure --enable-vt --with-cflags="-DRT_DEBUG_LEVEL=1" --with-builtin-crypto
//
// Simulation Topology:
// Scenario: MR and MNN moves from under AR1 to AR2 with Care-of-Address
//           alternation. during movement, MNN keeps ping6 to CN.
//
//                          The Internet
//                             |
//                        +----+------+
//                        |   host    |        Real World
//                        +-----------+
//  -------------------------  |exttap  ---------------------------
//                        +----+------+
//                        |tap(bridge)|        In Simulator
//                        +-----------+
//                             |
//                         (IPv4 only)
//                               sim0
//                        +----+------+
//                        |    MR     |
//                        +-----------+
//                             |sim1
//                        +---------+
//                        |   MNN   |
//                        +---------+

#include "ns3/dce-module.h"
#include "ns3/helper-module.h"
#include "ns3/simulator-module.h"
#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include <fstream>
#include "ns3/visualizer.h"


using namespace ns3;


static void RunIp (Ptr<Node> node, Time at, std::string str)
{
  DceApplicationHelper process;
  ApplicationContainer apps;
  process.SetBinary ("build/debug/ip");
  process.SetStackSize (1 << 16);
  process.ResetArguments ();
  process.ParseArguments (str.c_str ());
  apps = process.Install (node);
  apps.Start (at);
}

static void AddAddress (Ptr<Node> node, Time at, const char *name, const char *address)
{
  std::ostringstream oss;
  oss << "-f inet6 addr add " << address << " dev " << name;
  RunIp (node, at, oss.str ());
}

static void
CourseChangeCallback (std::string path, Ptr<const MobilityModel> model)
{
  Vector position = model->GetPosition ();
  //  std::cout << "CourseChange " << path << " x=" << position.x << ", y=" << position.y << ", z=" << position.z << std::endl;
}

bool useViz = false;
bool useTap = true;

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.AddValue ("useViz", "visualization", useViz);
  cmd.Parse (argc, argv);

  if (useTap)
    {
      GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));
      DynamicCast<RealtimeSimulatorImpl> (Simulator::GetImplementation ())->SetAttribute
              ("SynchronizationMode", EnumValue (RealtimeSimulatorImpl::SYNC_BEST_EFFORT));
    }

  NodeContainer mr, tapHost;
  mr.Create (1);
  tapHost.Create (1);
  NodeContainer mnn;
  mnn.Create (1);

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (mr);

  mobility.PushReferenceMobilityModel (mr.Get (0));
  Ptr<MobilityModel> parentMobility = mr.Get (0)->GetObject<MobilityModel> ();
  Vector pos =  parentMobility->GetPosition ();
  Ptr<ListPositionAllocator> positionAllocMnn =
    CreateObject<ListPositionAllocator> ();
  pos.x = 5;
  pos.y = 20;
  positionAllocMnn->Add (pos);
  mobility.SetPositionAllocator (positionAllocMnn);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (mnn);

  WifiHelper wifi;
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper phyChannel = YansWifiChannelHelper::Default ();
  NqosWifiMacHelper mac;
  CsmaHelper csma;
  phy.SetChannel (phyChannel.Create ());
  mac.SetType ("ns3::AdhocWifiMac");
  wifi.SetStandard (WIFI_STANDARD_80211a);

  NetDeviceContainer tapDevs = csma.Install (NodeContainer (tapHost.Get (0), mr));

  phy.SetChannel (phyChannel.Create ());
  NetDeviceContainer mnp_devices = wifi.Install (phy, mac, NodeContainer (mr.Get (0), mnn.Get (0)));

  phy.EnablePcapAll ("dsmip6d-tap-mr");
  csma.EnablePcapAll ("dsmip6d-tap-mr");

  DceManagerHelper processManager;
  processManager.SetNetworkStack ("ns3::LinuxSocketFdFactory",
                                  "Library", StringValue ("liblinux.so"));
  processManager.Install (mr);

  // Prefix configuration
  std::ostringstream oss;
  std::string mnp1 = "2001:1:2:5::1";
  std::string ha_addr = "2001:200:0:8801::3939";
  std::string mr_hoa = "2001:200:0:8801::3901";

  // For MR
  DceApplicationHelper process;
  ApplicationContainer apps;
  QuaggaHelper quagga;
  Mip6dHelper mip6d;
  for (uint32_t i = 0; i < mr.GetN (); i++)
    {
      RunIp (mr.Get (i), Seconds (0.11), "link set sim0 up");
      RunIp (mr.Get (i), Seconds (3.0), "link set ip6tnl0 up");
      oss << mnp1 << "/64";
      AddAddress (mr.Get (i), Seconds (0.12), "sim1", oss.str ().c_str ());
      RunIp (mr.Get (i), Seconds (0.13), "link set sim1 up");

      RunIp (mr.Get (i), Seconds (4.2), "addr list");
      RunIp (mr.Get (i), Seconds (20.0), "route show table all");

      mip6d.AddMobileNetworkPrefix (mr.Get (i), Ipv6Address (mnp1.c_str ()), Ipv6Prefix (64));
      mip6d.AddHomeAgentAddress (mr.Get (i), Ipv6Address (ha_addr.c_str ()));
      mip6d.AddHomeAddress (mr.Get (i), Ipv6Address (mr_hoa.c_str ()), Ipv6Prefix (64));
      mip6d.AddEgressInterface (mr.Get (i), "sim0");
    }
  mip6d.EnableMR (mr);
  mip6d.Install (mr);

  quagga.EnableRadvd (mr.Get (0), "sim1");
  quagga.EnableZebraDebug (mr);
  quagga.Install (mr);


  // TAP Host
  if (useTap)
    {
      TapBridgeHelper tapBridge (Ipv4Address ("0.0.0.1"));
      tapBridge.SetAttribute ("Mode", StringValue ("UseLocal"));
      tapBridge.SetAttribute ("DeviceName", StringValue ("exttap"));
      tapBridge.Install (tapHost.Get (0), tapDevs.Get (0));
    }

  // MNN
  if (1)
    {
      LogComponentEnable ("Ping6Application", LOG_LEVEL_INFO);
      // Ping6
      /* Install IPv4/IPv6 stack */
      InternetStackHelper internetv6;
      internetv6.SetIpv4StackInstall (false);
      internetv6.Install (mnn);

      Ipv6AddressHelper ipv6;
      Ipv6InterfaceContainer i1 = ipv6.AssignWithoutAddress (mnp_devices.Get (1));

      uint32_t packetSize = 1024;
      uint32_t maxPacketCount = 50000000;
      Time interPacketInterval = Seconds (1.);
      Ping6Helper ping6;

      ping6.SetLocal (Ipv6Address::GetAny ());
      ping6.SetRemote (Ipv6Address ("2001:1:2:6::1"));

      ping6.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
      ping6.SetAttribute ("Interval", TimeValue (interPacketInterval));
      ping6.SetAttribute ("PacketSize", UintegerValue (packetSize));
      ApplicationContainer apps = ping6.Install (mnn.Get (0));
      apps.Start (Seconds (2.0));
      //      apps.Stop (Seconds (100.0));
    }

  Config::Connect ("/NodeList/*/$ns3::MobilityModel/CourseChange", MakeCallback (&CourseChangeCallback));

  if (useViz)
    {
      Visualizer::Run ();
    }
  else
    {
      Simulator::Stop (Seconds (600.0));
      Simulator::Run ();
    }
  Simulator::Destroy ();

  return 0;
}

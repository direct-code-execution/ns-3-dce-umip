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
//                                    +-----------+
//                                    |    HA     |
//                                    +-----------+
//                                         |sim0
//                              +----------+------------+
//                              |sim0                   |sim0
//      +--------+     sim2+----+---+              +----+---+
//      |   CN   |  - - - -|   AR1  |              |   AR2  |
//      +--------+         +---+----+              +----+---+
//                             |sim1                    |sim1
//                             |                        |
//
//                               sim0                     sim0
//                        +----+------+  (Movement) +----+-----+
//                        |    MR     |   <=====>   |    MR    |
//                        +-----------+             +----------+
//                             |sim1                     |sim1
//                        +---------+               +---------+
//                        |   MNN   |               |   MNN   |
//                        +---------+               +---------+

#include "ns3/network-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/dce-module.h"
#include "ns3/mip6d-helper.h"
#include "ns3/csma-helper.h"
#include "ns3/wifi-helper.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/nqos-wifi-mac-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/ping6-helper.h"
#include "ns3/quagga-helper.h"


using namespace ns3;

static void RunIp (Ptr<Node> node, Time at, std::string str)
{
  DceApplicationHelper process;
  ApplicationContainer apps;
  process.SetBinary ("ip");
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

bool usePing = true;
int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.AddValue ("usePing", "Using Ping6 or not", usePing);
  cmd.Parse (argc, argv);

  NodeContainer mr, ha, ar;
  ha.Create (1);
  ar.Create (2);
  mr.Create (1);
  NodeContainer mnn, cn;
  cn.Create (1);
  mnn.Create (1);

  NetDeviceContainer devices;
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (75.0, -50.0, 0.0)); // HA
  positionAlloc->Add (Vector (0.0, 10.0, 0.0)); // AR1
  positionAlloc->Add (Vector (150.0, 10.0, 0.0)); // AR2
  positionAlloc->Add (Vector (-50.0, 10.0, 0.0)); // CN
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (ha);
  mobility.Install (ar);
  mobility.Install (cn);

  Ptr<ns3::RandomDiscPositionAllocator> r_position =
    CreateObject<RandomDiscPositionAllocator> ();
  r_position->SetX (100);
  r_position->SetY (50);
  r_position->SetRho (UniformVariable (200, 0));
  mobility.SetPositionAllocator (r_position);
  mobility.SetMobilityModel ("ns3::RandomDirection2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (0, 200, 30, 60)),
                             "Speed", RandomVariableValue (ConstantVariable (10)),
                             "Pause", RandomVariableValue (ConstantVariable (0.2)));
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

  WifiHelper wifi = WifiHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper phyChannel = YansWifiChannelHelper::Default ();
  NqosWifiMacHelper mac;
  CsmaHelper csma;
  phy.SetChannel (phyChannel.Create ());
  mac.SetType ("ns3::AdhocWifiMac");
  wifi.SetStandard (WIFI_PHY_STANDARD_80211a);

  devices = csma.Install (NodeContainer (ar.Get (0), ha.Get (0), ar.Get (1)));

  phy.SetChannel (phyChannel.Create ());
  devices = wifi.Install (phy, mac, NodeContainer (ar.Get (0), mr, ar.Get (1)));

  phy.SetChannel (phyChannel.Create ());
  NetDeviceContainer mnp_devices = wifi.Install (phy, mac, NodeContainer (mr.Get (0), mnn.Get (0)));

  NetDeviceContainer cn_devices = csma.Install (NodeContainer (ar.Get (0), cn.Get (0)));

  DceManagerHelper processManager;
  //  processManager.SetLoader ("ns3::DlmLoaderFactory");
  processManager.SetTaskManagerAttribute ("FiberManagerType",
                                          EnumValue (0));
  processManager.SetNetworkStack ("ns3::LinuxSocketFdFactory",
                                  "Library", StringValue ("libnet-next-2.6.so"));
  processManager.Install (mr);
  processManager.Install (ha);
  processManager.Install (ar);

  // Prefix configuration
  std::string ha_sim0 ("2001:1:2:3::1/64");
  std::string mnp1 ("2001:1:2:5::1");
  std::string mnp2 ("2001:1:2:8::1");

  std::vector <std::string> *mnps = new std::vector <std::string>;
  mnps->push_back (mnp1);
  mnps->push_back (mnp2);

  // For HA
  AddAddress (ha.Get (0), Seconds (0.1), "sim0", ha_sim0.c_str ());
  RunIp (ha.Get (0), Seconds (0.11), "link set lo up");
  RunIp (ha.Get (0), Seconds (0.11), "link set sim0 up");
  RunIp (ha.Get (0), Seconds (3.0), "link set ip6tnl0 up");
  RunIp (ha.Get (0), Seconds (3.1), "addr list");
  //  RunIp (ha.Get (0), Seconds (3.2), "-6 route add default via 2001:1:2:3::2 dev sim0");
  RunIp (ha.Get (0), Seconds (3.15), "-6 route add 2001:1:2:4::/64 via 2001:1:2:3::2 dev sim0");
  RunIp (ha.Get (0), Seconds (3.15), "-6 route add 2001:1:2:6::/64 via 2001:1:2:3::2 dev sim0");
  RunIp (ha.Get (0), Seconds (3.15), "-6 route add 2001:1:2:7::/64 via 2001:1:2:3::3 dev sim0");

  // For AR1 (the intermediate node)
  AddAddress (ar.Get (0), Seconds (0.1), "sim0", "2001:1:2:3::2/64");
  RunIp (ar.Get (0), Seconds (0.11), "link set lo up");
  RunIp (ar.Get (0), Seconds (0.11), "link set sim0 up");
  AddAddress (ar.Get (0), Seconds (0.12), "sim1", "2001:1:2:4::2/64");
  RunIp (ar.Get (0), Seconds (0.13), "link set sim1 up");
  AddAddress (ar.Get (0), Seconds (0.13), "sim2", "2001:1:2:6::2/64");
  RunIp (ar.Get (0), Seconds (0.14), "link set sim2 up");
  RunIp (ar.Get (0), Seconds (0.15), "-6 route add 2001:1:2::/48 via 2001:1:2:3::1 dev sim0");
  RunIp (ar.Get (0), Seconds (0.15), "route show table all");
  Ptr<LinuxSocketFdFactory> kern = ar.Get (0)->GetObject<LinuxSocketFdFactory>();
  Simulator::ScheduleWithContext (ar.Get (0)->GetId (), Seconds (0.1),
                                  MakeEvent (&LinuxSocketFdFactory::Set, kern,
                                             ".net.ipv6.conf.all.forwarding", "1"));

  // For AR2 (the intermediate node)
  AddAddress (ar.Get (1), Seconds (0.1), "sim0", "2001:1:2:3::3/64");
  RunIp (ar.Get (1), Seconds (0.11), "link set lo up");
  RunIp (ar.Get (1), Seconds (0.11), "link set sim0 up");
  AddAddress (ar.Get (1), Seconds (0.12), "sim1", "2001:1:2:7::2/64");
  RunIp (ar.Get (1), Seconds (0.13), "link set sim1 up");
  std::ostringstream oss;
  oss << "-6 route add " << mnp1 << "/64 via 2001:1:2:3::1 dev sim0";
  RunIp (ar.Get (1), Seconds (0.15), oss.str ());
  RunIp (ar.Get (1), Seconds (0.15), "route show table all");
  kern = ar.Get (1)->GetObject<LinuxSocketFdFactory>();
  Simulator::ScheduleWithContext (ar.Get (1)->GetId (), Seconds (0.1),
                                  MakeEvent (&LinuxSocketFdFactory::Set, kern,
                                             ".net.ipv6.conf.all.forwarding", "1"));

  // For MR
  for (uint32_t i = 0; i < mr.GetN (); i++)
    {
      RunIp (mr.Get (i), Seconds (0.11), "link set lo up");
      RunIp (mr.Get (i), Seconds (0.11), "link set sim0 up");
      RunIp (mr.Get (i), Seconds (3.0), "link set ip6tnl0 up");
      //      RunIp (mr.Get (i), Seconds (3.1), "addr list");
      oss.str ("");
      oss << mnps->at (i) << "/64";
      AddAddress (mr.Get (i), Seconds (0.12), "sim1", oss.str ().c_str ());
      RunIp (mr.Get (i), Seconds (0.13), "link set sim1 up");
    }

  RunIp (ha.Get (0), Seconds (4.0), "addr list");
  RunIp (ar.Get (0), Seconds (4.1), "addr list");
  RunIp (mr.Get (0), Seconds (4.2), "addr list");
  RunIp (ha.Get (0), Seconds (20.0), "route show table all");
  RunIp (mr.Get (0), Seconds (20.0), "route show table all");

  {
    ApplicationContainer apps;
    QuaggaHelper quagga;
    Mip6dHelper mip6d;

    // HA
    mip6d.AddHaServedPrefix (ha.Get (0), Ipv6Address ("2001:1:2::"), Ipv6Prefix (48));
    mip6d.EnableHA (ha);
    mip6d.Install (ha);

    // MR
    for (uint32_t i = 0; i < mr.GetN (); i++)
      {
        mip6d.AddMobileNetworkPrefix (mr.Get (i), Ipv6Address (mnps->at (i).c_str ()), Ipv6Prefix (64));
        std::string ha_addr = ha_sim0;
        ha_addr.replace (ha_addr.find ("/"), 3, "\0  ");
        mip6d.AddHomeAgentAddress (mr.Get (i), Ipv6Address (ha_addr.c_str ()));
        mip6d.AddHomeAddress (mr.Get (i), Ipv6Address ("2001:1:2:3::1000"), Ipv6Prefix (64));
        mip6d.AddEgressInterface (mr.Get (i), "sim0");
      }
    mip6d.EnableMR (mr);
    mip6d.Install (mr);

    quagga.EnableRadvd (mr.Get (0), "sim1", "2001:1:2:5::/64");
    quagga.EnableZebraDebug (mr);
    quagga.Install (mr);

    // AR
    quagga.EnableRadvd (ar.Get (0), "sim0", "2001:1:2:3::/64");
    quagga.EnableHomeAgentFlag (ar.Get (0), "sim0");
    quagga.EnableRadvd (ar.Get (0), "sim1", "2001:1:2:4::/64");
    quagga.EnableRadvd (ar.Get (0), "sim2", "2001:1:2:6::/64");
    quagga.EnableRadvd (ar.Get (1), "sim1", "2001:1:2:7::/64");
    quagga.EnableZebraDebug (ar);
    quagga.Install (ar);
  }

  // MNN
  if (usePing)
    {
      LogComponentEnable ("Ping6Application", LOG_LEVEL_INFO);
      // Ping6
      /* Install IPv4/IPv6 stack */
      InternetStackHelper internetv6;
      internetv6.SetIpv4StackInstall (false);
      internetv6.Install (mnn);
      internetv6.Install (cn);

      Ipv6AddressHelper ipv6;
      Ipv6InterfaceContainer i1 = ipv6.AssignWithoutAddress (mnp_devices.Get (1));

      ipv6.NewNetwork (Ipv6Address ("2001:1:2:6::"), 64);
      Ipv6InterfaceContainer i2 = ipv6.Assign (cn_devices.Get (1));

      Ptr<Ipv6StaticRouting> routing = 0;
      Ipv6StaticRoutingHelper routingHelper;
      routing = routingHelper.GetStaticRouting (cn.Get (0)->GetObject<Ipv6> ());
      routing->SetDefaultRoute (Ipv6Address ("2001:1:2:6::2"), 1, Ipv6Address ("::"), 0);

      uint32_t packetSize = 1024;
      uint32_t maxPacketCount = 50000000;
      Time interPacketInterval = Seconds (1.);
      Ping6Helper ping6;

      ping6.SetLocal (Ipv6Address::GetAny ());
      ping6.SetRemote (i2.GetAddress (0, 1));

      ping6.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
      ping6.SetAttribute ("Interval", TimeValue (interPacketInterval));
      ping6.SetAttribute ("PacketSize", UintegerValue (packetSize));
      ApplicationContainer apps = ping6.Install (mnn.Get (0));
      apps.Start (Seconds (2.0));
    }

  phy.EnablePcapAll ("dce-mip6d");
  csma.EnablePcapAll ("dce-mip6d");

  Simulator::Stop (Seconds (30.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}

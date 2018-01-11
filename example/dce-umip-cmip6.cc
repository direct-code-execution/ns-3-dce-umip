/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

//
// MIPv6 (Mobile IPv6) simulation with umip (mip6d) and net-next-2.6.
//
// UMIP: http://www.umip.org/git/umip.git
// patchset: 0daa3924177f326e26ed8bbb9dc9f0cdf8a51618
// build:  CFLAGS="-fPIC -g" CXXFLAGS="-fPIC -g" LDFLAGS="-pie -g" ./configure --enable-vt --with-cflags="-DRT_DEBUG_LEVEL=1" --with-builtin-crypto
//
// Simulation Topology:
// Scenario: MN moves from under AR1 to AR2 with Care-of-Address
//           alternation. during movement, MN keeps ping6 to CN.
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
//                           :::::
//                             |sim0                    |sim0
//                        +---------+   (Movement)  +--------+
//                        |    MN   |     <=====>   |   MN   |
//                        +---------+               +--------+

#include "ns3/network-module.h"
#include "ns3/core-module.h"
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

  NodeContainer mn, ha, ar;
  ha.Create (1);
  ar.Create (2);
  mn.Create (1);
  NodeContainer cn;
  cn.Create (1);

  NetDeviceContainer devices;
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (75.0, -50.0, 0.0)); // HA
  positionAlloc->Add (Vector (0.0, 10.0, 0.0)); // AR1
  positionAlloc->Add (Vector (300.0, 10.0, 0.0)); // AR2
  positionAlloc->Add (Vector (-50.0, 10.0, 0.0)); // CN
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (ha);
  mobility.Install (ar);
  mobility.Install (cn);

  Ptr<ns3::RandomDiscPositionAllocator> r_position =
    CreateObject<RandomDiscPositionAllocator> ();
  Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();                                x->SetAttribute ("Min", DoubleValue (0.0));
  x->SetAttribute ("Max", DoubleValue (200.0));
  r_position->SetX (100);
  r_position->SetY (50);
  r_position->SetRho (x);
  mobility.SetPositionAllocator (r_position);
  mobility.SetMobilityModel ("ns3::RandomDirection2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (0, 200, 30, 60)),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=10.0]"),
                             "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.2]"));
  //  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (mn);


  WifiHelper wifi;
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper phyChannel = YansWifiChannelHelper::Default ();
  NqosWifiMacHelper mac;
  CsmaHelper csma;
  phy.SetChannel (phyChannel.Create ());
  mac.SetType ("ns3::AdhocWifiMac");
  wifi.SetStandard (WIFI_PHY_STANDARD_80211a);

  devices = csma.Install (NodeContainer (ar.Get (0), ha.Get (0), ar.Get (1)));

  phy.SetChannel (phyChannel.Create ());
  devices = wifi.Install (phy, mac, NodeContainer (ar.Get (0), mn, ar.Get (1)));

  NetDeviceContainer cn_devices = csma.Install (NodeContainer (ar.Get (0), cn.Get (0)));

  DceManagerHelper dceMng;
  DceApplicationHelper dce;
  // dceMng.SetLoader ("ns3::DlmLoaderFactory");
  dceMng.SetTaskManagerAttribute ("FiberManagerType",
                                          EnumValue (0));
  dceMng.SetNetworkStack ("ns3::LinuxSocketFdFactory",
                       "Library", StringValue ("liblinux.so"));
  dceMng.Install (mn);
  dceMng.Install (ha);
  dceMng.Install (ar);
  dceMng.Install (cn);

  // Prefix configuration
  std::string ha_sim0 ("2001:1:2:3::1/64");

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
  AddAddress (ar.Get (0), Seconds (0.1), "sim1", "2001:1:2:4::2/64");
  AddAddress (ar.Get (0), Seconds (0.1), "sim2", "2001:1:2:6::2/64");
  RunIp (ar.Get (0), Seconds (0.11), "link set lo up");
  RunIp (ar.Get (0), Seconds (0.11), "link set sim0 up");
  RunIp (ar.Get (0), Seconds (0.13), "link set sim1 up");
  RunIp (ar.Get (0), Seconds (0.14), "link set sim2 up");
  RunIp (ar.Get (0), Seconds (0.15), "-6 route add 2001:1:2::/48 via 2001:1:2:3::1 dev sim0");
  RunIp (ar.Get (0), Seconds (0.15), "route show table all");
  Ptr<LinuxSocketFdFactory> kern = ar.Get (0)->GetObject<LinuxSocketFdFactory>();
  Simulator::ScheduleWithContext (ar.Get (0)->GetId (), Seconds (0.1),
                                  MakeEvent (&LinuxSocketFdFactory::Set, kern,
                                             ".net.ipv6.conf.all.forwarding", "1"));

  // For AR2 (the intermediate node)
  AddAddress (ar.Get (1), Seconds (0.1), "sim0", "2001:1:2:3::3/64");
  AddAddress (ar.Get (1), Seconds (0.1), "sim1", "2001:1:2:7::2/64");
  RunIp (ar.Get (1), Seconds (0.11), "link set lo up");
  RunIp (ar.Get (1), Seconds (0.11), "link set sim0 up");
  RunIp (ar.Get (1), Seconds (0.13), "link set sim1 up");
  RunIp (ar.Get (1), Seconds (0.15), "route show table all");
  kern = ar.Get (1)->GetObject<LinuxSocketFdFactory>();
  Simulator::ScheduleWithContext (ar.Get (1)->GetId (), Seconds (0.1),
                                  MakeEvent (&LinuxSocketFdFactory::Set, kern,
                                             ".net.ipv6.conf.all.forwarding", "1"));

  // For MN
  RunIp (mn.Get (0), Seconds (0.11), "link set lo up");
  RunIp (mn.Get (0), Seconds (0.11), "link set sim0 up");
  RunIp (mn.Get (0), Seconds (3.0), "link set ip6tnl0 up");
  //      RunIp (mn.Get (0), Seconds (3.1), "addr list");

  // For CN
  RunIp (cn.Get (0), Seconds (0.11), "link set lo up");
  RunIp (cn.Get (0), Seconds (1.11), "link set sim0 up");
  RunIp (cn.Get (0), Seconds (1.11), "add default via 2001:1:2:6::2");
  AddAddress (cn.Get (0), Seconds (0.12), "sim0", "2001:1:2:6::7/64");

  RunIp (ha.Get (0), Seconds (4.0), "addr list");
  RunIp (ar.Get (0), Seconds (4.1), "addr list");
  RunIp (ar.Get (1), Seconds (37.0), "addr list");
  RunIp (mn.Get (0), Seconds (40.2), "addr list");
  RunIp (ha.Get (0), Seconds (20.0), "route show table all");
  RunIp (mn.Get (0), Seconds (50.0), "route show table all");

  {
    ApplicationContainer apps;
    QuaggaHelper quagga;
    Mip6dHelper mip6d;

    // HA
    mip6d.EnableHA (ha);
    mip6d.Install (ha);

    // MN
    std::string ha_addr = ha_sim0;
    ha_addr.replace (ha_addr.find ("/"), 3, "\0  ");
    mip6d.AddHomeAgentAddress (mn.Get (0), Ipv6Address (ha_addr.c_str ()));
    mip6d.AddHomeAddress (mn.Get (0), Ipv6Address ("2001:1:2:3::1000"), Ipv6Prefix (64));
    mip6d.AddEgressInterface (mn.Get (0), "sim0");
    mip6d.Install (mn);

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
      // Ping6
      uint32_t packetSize = 1024;
      uint32_t maxPacketCount = 50000000;
      Time interPacketInterval = Seconds (1.);

      dce.SetBinary ("ping6");
      dce.SetStackSize (1 << 16);
      dce.ResetArguments ();
      dce.ResetEnvironment ();
      // dce.AddArgument ("-i");
      // dce.AddArgument (interPacketInterval.GetSeconds ());
      dce.AddArgument ("2001:1:2:6::7");
      ApplicationContainer apps = dce.Install (mn.Get (0));
      apps.Start (Seconds (50.0));
    }

  phy.EnablePcapAll ("dce-umip-cmip6");
  csma.EnablePcapAll ("dce-umip-cmip6");

  Simulator::Stop (Seconds (300.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}

/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

//
// PMIP (Proxy Mobile IPv6) simulation with umip (mip6d) and net-next-2.6.
//
// UMIP: http://www.umip.org/git/umip.git
// patchset: ???
// build:  CFLAGS="-fPIC -g" CXXFLAGS="-fPIC -g" LDFLAGS="-pie -g" ./configure --enable-vt --with-cflags="-DRT_DEBUG_LEVEL=1" --with-builtin-crypto
//
// Simulation Topology:
// 
//                           +---------+       
//                           |   CN    |       
//                           +---------+       
//                               |sim0 (::a)
//                               +  2001:a:a:0::/64
//                               |sim0 (::1)
//                           +---+-----+
//                           |   LMA   |      
//                           +---------+
//                               |sim1 (::1)
//                               + 2001:a:b:0::/64
//                               |sim0 (::a)
//                           +---+-----+
//                           |   MAG   |      
//                           +---------+
//                               |sim1 (fe80::a)
//                               + 2001:a:b:1::/64
//                               |sim0 (HNP::MN)
//                           +---+-----+
//                           |   MN   |      
//                           +---------+
//

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


using namespace ns3;

static void RunIp (Ptr<Node> node, Time at, std::string str)
{
  DceApplicationHelper process;
  ApplicationContainer apps;
  process.SetBinary ("ip");
  process.SetStackSize (1<<16);
  process.ResetArguments();
  process.ParseArguments(str.c_str ());
  apps = process.Install (node);
  apps.Start (at);
}

static void AddAddress (Ptr<Node> node, Time at, const char *name, const char *address)
{
  std::ostringstream oss;
  oss << "-f inet6 addr add " << address << " dev " << name;
  RunIp (node, at, oss.str ());
}



int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer mag, lma;
  lma.Create (1);
  mag.Create (1);
  NodeContainer mn, cn;
  cn.Create (1);
  mn.Create (1);

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 50.0, 0.0)); // LMA
  positionAlloc->Add (Vector (0.0, 100.0, 0.0)); // MAG
  positionAlloc->Add (Vector (0.0, 0.0, 0.0)); // CN
  positionAlloc->Add (Vector (0.0, 150.0, 0.0)); // MN
  mobility.SetPositionAllocator (positionAlloc);
  mobility.Install (lma);
  mobility.Install (mag);
  mobility.Install (cn);
  mobility.Install (mn);

  CsmaHelper csma;
  WifiHelper wifi;
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper phyChannel = YansWifiChannelHelper::Default ();
  NqosWifiMacHelper mac;
  mac.SetType ("ns3::AdhocWifiMac");
  wifi.SetStandard (WIFI_PHY_STANDARD_80211a);

  NetDeviceContainer cn_devices = csma.Install (NodeContainer (lma.Get (0), cn.Get (0)));

  csma.Install (NodeContainer (lma.Get (0), mag.Get (0)));

  phy.SetChannel (phyChannel.Create ());
  NetDeviceContainer mn_devices = wifi.Install (phy, mac, NodeContainer (mag.Get (0), mn.Get (0)));

  phy.EnablePcapAll ("mip6d-pmip");
  csma.EnablePcapAll ("mip6d-pmip");

  DceManagerHelper processManager;
  processManager.SetTaskManagerAttribute ("FiberManagerType", 
                                          EnumValue (0));
  processManager.SetLoader ("ns3::DlmLoaderFactory");
  processManager.SetNetworkStack("ns3::LinuxSocketFdFactory",
				 "Library", StringValue ("liblinux.so"));
  processManager.Install (lma);
  processManager.Install (mag);

  // Prefix configuration
  std::string lma_sim0 ("2001:a:a:0::1/64");
  std::string lma_sim1 ("2001:a:b:0::1/64");
  std::string mag_sim0 ("2001:a:b:0::a/64");
  std::string hnp ("2001:a:b:1::1");

  // For LMA
  AddAddress (lma.Get (0), Seconds (0.1), "sim0", lma_sim0.c_str ());
  AddAddress (lma.Get (0), Seconds (0.11), "sim1", lma_sim1.c_str ());
  RunIp (lma.Get (0), Seconds (0.21), "link set sim0 up");
  RunIp (lma.Get (0), Seconds (0.22), "link set sim1 promisc on");
  RunIp (lma.Get (0), Seconds (0.23), "link set sim1 up");
  RunIp (lma.Get (0), Seconds (3.0), "link set ip6tnl0 up");
  RunIp (lma.Get (0), Seconds (3.1), "addr list");

  // For MAG
  AddAddress (mag.Get (0), Seconds (0.1), "sim0", mag_sim0.c_str ());
  RunIp (mag.Get (0), Seconds (0.21), "link set sim0 up");
  RunIp (mag.Get (0), Seconds (0.22), "link set sim1 promisc on");
  RunIp (mag.Get (0), Seconds (0.23), "link set sim1 up");
  RunIp (mag.Get (0), Seconds (3.0), "link set ip6tnl0 up");
  RunIp (mag.Get (0), Seconds (3.1), "addr list");


  RunIp (lma.Get (0), Seconds (20.0), "route show table all");
  RunIp (mag.Get (0), Seconds (20.0), "route show table all");

  {
    DceApplicationHelper process;
    ApplicationContainer apps;
    Mip6dHelper mip6d;

    // LMA
    //    mip6d.AddHaServedPrefix (lma.Get (0), Ipv6Address ("2001:1:2::"), Ipv6Prefix (48));
    mip6d.EnableLMA (lma.Get (0), "sim0");
    mip6d.SetBinary (lma, "mip6d.pmip");
    mip6d.Install (lma);

    // MAG
    std::string mag_addr = mag_sim0;
    mag_addr.replace (mag_addr.find ("/"), 3, "\0  ");
    mip6d.EnableMAG (mag.Get (0), "sim0", Ipv6Address (mag_addr.c_str ()));

    std::string lma_addr = lma_sim1;
    lma_addr.replace (lma_addr.find ("/"), 3, "\0  ");
    mip6d.AddMNProfileMAG (mag.Get (0), Mac48Address ("00:00:00:00:00:06"),
                           lma_addr.c_str (), 
                           Ipv6Address (hnp.c_str ()), Ipv6Prefix (64));
    mip6d.SetBinary (mag, "mip6d.pmip");
    mip6d.Install (mag);
  }

  // MNN
  if (1)
    {
      LogComponentEnable ("Ping6Application", LOG_LEVEL_INFO);
      // Ping6
      /* Install IPv4/IPv6 stack */
      InternetStackHelper internetv6;
      internetv6.SetIpv4StackInstall (false);
      internetv6.Install (mn);
      internetv6.Install (cn);

      Ipv6AddressHelper ipv6;
      Ipv6InterfaceContainer i1 = ipv6.Assign (mn_devices.Get (1));

      ipv6.SetBase (Ipv6Address ("2001:a:a:0::"), 64);
      Ipv6InterfaceContainer i2 = ipv6.Assign (cn_devices.Get (1));

      Ptr<Ipv6StaticRouting> routing = 0;
      Ipv6StaticRoutingHelper routingHelper;
      routing = routingHelper.GetStaticRouting (cn.Get (0)->GetObject<Ipv6> ());
      routing->SetDefaultRoute (Ipv6Address ("2001:a:a:0::1"), 1, Ipv6Address ("::"), 0);

      uint32_t packetSize = 1024;
      uint32_t maxPacketCount = 50000000;
      Time interPacketInterval = Seconds (1.);
      Ping6Helper ping6;

      ping6.SetLocal (Ipv6Address::GetAny ());
      ping6.SetRemote (i2.GetAddress (0, 1));

      ping6.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
      ping6.SetAttribute ("Interval", TimeValue (interPacketInterval));
      ping6.SetAttribute ("PacketSize", UintegerValue (packetSize));
      ApplicationContainer apps = ping6.Install (mn.Get (0));
      apps.Start (Seconds (2.0));
      //      apps.Stop (Seconds (100.0));
    }

  Simulator::Stop (Seconds (200.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}

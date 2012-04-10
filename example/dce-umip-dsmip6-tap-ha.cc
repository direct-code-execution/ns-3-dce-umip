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
//                        +-----------+
//                        |    HA     |
//                        +-----------+
//                             |sim0
//                        +----+------+
//                        |tap(bridge)|        In Simulator
//                        +-----------+
//                             |
//  -------------------------  |exttap  ---------------------------
//                        +----+------+
//                        |   host    |        Real World
//                        +-----------+
//                             |
//                          The Internet

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

  NodeContainer  ha, tapHost;
  ha.Create (1);
  tapHost.Create (1);
  NodeContainer cn;
  cn.Create (1);

  CsmaHelper csma;
  NetDeviceContainer tapDevs = csma.Install (NodeContainer (tapHost.Get (0), ha));

  NetDeviceContainer cn_devices;
  // = csma.Install (NodeContainer (ar.Get (0), cn.Get (0)));

  csma.EnablePcapAll ("dsmip6d-tap-ha");

  DceManagerHelper processManager;
  processManager.SetNetworkStack ("ns3::LinuxSocketFdFactory",
                                  "Library", StringValue ("libnet-next-2.6.so"));
  processManager.Install (ha);

  // Prefix configuration
  std::string ha_sim0 ("2001:200:0:8801::3939/64");

  // For HA
  AddAddress (ha.Get (0), Seconds (0.1), "sim0", ha_sim0.c_str ());
  RunIp (ha.Get (0), Seconds (0.11), "link set lo up");
  RunIp (ha.Get (0), Seconds (0.11), "link set sim0 up");
  RunIp (ha.Get (0), Seconds (3.0), "link set ip6tnl0 up");
  RunIp (ha.Get (0), Seconds (3.1), "addr list");
  RunIp (ha.Get (0), Seconds (3.2), "-6 route add default via fe80::212:e2ff:fe28:130e dev sim0");
  RunIp (ha.Get (0), Seconds (4.13), "-4 addr add 192.168.10.1/24 dev sim0");
  RunIp (ha.Get (0), Seconds (4.15), "-4 route add default via 192.168.10.2 dev sim0");

  RunIp (ha.Get (0), Seconds (4.0), "addr list");
  RunIp (ha.Get (0), Seconds (20.0), "route show table all");

  {
    DceApplicationHelper process;
    ApplicationContainer apps;
    QuaggaHelper quagga;
    Mip6dHelper mip6d;

    // HA
    mip6d.AddHaServedPrefix (ha.Get (0), Ipv6Address ("2001:1:2::"), Ipv6Prefix (48));
    mip6d.EnableHA (ha);
    mip6d.EnableDebug (ha);
    mip6d.Install (ha);

  }

  // CN
  if (0)
    {
      // Ping6
      /* Install IPv4/IPv6 stack */
      InternetStackHelper internetv6;
      internetv6.SetIpv4StackInstall (false);
      internetv6.Install (cn);

      Ipv6AddressHelper ipv6;
      ipv6.NewNetwork (Ipv6Address ("2001:1:2:6::"), 64);
      Ipv6InterfaceContainer i2 = ipv6.Assign (cn_devices.Get (1));

      Ptr<Ipv6StaticRouting> routing = 0;
      Ipv6StaticRoutingHelper routingHelper;
      routing = routingHelper.GetStaticRouting (cn.Get (0)->GetObject<Ipv6> ());
      routing->SetDefaultRoute (Ipv6Address ("2001:1:2:6::2"), 1, Ipv6Address ("::"), 0);
    }

  // TAP Host
  if (useTap)
    {
      TapBridgeHelper tapBridge (Ipv4Address ("0.0.0.1"));
      tapBridge.SetAttribute ("Mode", StringValue ("UseLocal"));
      tapBridge.SetAttribute ("DeviceName", StringValue ("exttap"));
      tapBridge.Install (tapHost.Get (0), tapDevs.Get (0));
    }

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

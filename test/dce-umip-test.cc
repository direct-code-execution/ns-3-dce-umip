/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2012 Hajime Tazaki, NICT
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Simulation Topology:
 * Scenario: MR and MNN moves from under AR1 to AR2 with Care-of-Address
 *           alternation. during movement, MNN keeps ping6 to CN.
 *
 *                                    +-----------+
 *                                    |    HA     |
 *                                    +-----------+
 *                                         |sim0
 *                              +----------+------------+
 *                              |sim0                   |sim0
 *      +--------+     sim2+----+---+              +----+---+
 *      |   CN   |  - - - -|   AR1  |              |   AR2  |
 *      +--------+         +---+----+              +----+---+
 *                             |sim1                    |sim1
 *                             |                        |
 *
 *                               sim0                     sim0
 *                        +----+------+  (Movement) +----+-----+
 *                        |    MR     |   <=====>   |    MR    |
 *                        +-----------+             +----------+
 *                             |sim1                     |sim1
 *                        +---------+               +---------+
 *                        |   MNN   |               |   MNN   |
 *                        +---------+               +---------+
 * Author: Hajime Tazaki <tazaki@nict.go.jp>
 */

#include "ns3/network-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/dce-module.h"
#include "ns3/quagga-helper.h"
#include "ns3/wifi-helper.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/nqos-wifi-mac-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-helper.h"
#include "ns3/mip6d-helper.h"
#include "ns3/ping6-helper.h"
#include "ns3/ethernet-header.h"

#define OUTPUT(x)                                                       \
  {                                                                     \
    std::ostringstream oss;                                             \
    oss << "file=" << __FILE__ << " line=" << __LINE__ << " "           \
        << x << std::endl;                                              \
    std::string s = oss.str ();                                         \
    std::cerr << s.c_str ();                                            \
  }


static std::string g_testError;

extern "C" void dce_umip_test_store_test_error (const char *s)
{
  g_testError = s;
}

using namespace ns3;
namespace ns3 {

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

class DceUmipTestCase : public TestCase
{
public:
  DceUmipTestCase (std::string testname, Time maxDuration);
  void WifiRxCallback (std::string context, Ptr<const Packet> packet);
private:
  virtual void DoRun (void);
  static void Finished (int *pstatus, uint16_t pid, int status);

  std::string m_testname;
  Time m_maxDuration;
  bool m_pingStatus;
  bool m_debug;
};

void
DceUmipTestCase::WifiRxCallback (std::string context, Ptr<const Packet> originalPacket)
{
  if (m_pingStatus)
    {
      return;
    }
  Ptr<Packet> packet = originalPacket->Copy ();

  Ipv4Header v4hdr;
  Icmpv4Header icmphdr;
  Ipv6Header v6hdr;
  Icmpv6Header icmp6hdr;

#if 0
  if (packet->PeekHeader (v4hdr) != 0)
    {
      packet->RemoveHeader (v4hdr);
      packet->RemoveHeader (icmphdr);
      if (icmphdr.GetType () == Icmpv4Header::ECHO_REPLY)
        {
          m_pingStatus = true;
        }
    }
  else 
#endif
    if (packet->PeekHeader (v6hdr) != 0)
    {
      packet->RemoveHeader (v6hdr);
      // if the next header is ipv6, it's ip6-in-ip6
      if (v6hdr.GetNextHeader () == Ipv6Header::IPV6_IPV6)
        {
          Ipv6Header v6hdr_2;
          packet->RemoveHeader (v6hdr_2);
        }
      packet->RemoveHeader (icmp6hdr);
      if (icmp6hdr.GetType () == Icmpv6Header::ICMPV6_ECHO_REPLY)
        {
          m_pingStatus = true;
        }
    }

  if (m_debug)
    {
      std::cout << context << " " << packet << std::endl;
      packet->Print (std::cout);
      std::cout << std::endl;
    }
}

DceUmipTestCase::DceUmipTestCase (std::string testname, Time maxDuration)
  : TestCase ("Check that process \"" + testname
              + "\" completes correctly."),
    m_testname (testname),
    m_maxDuration (maxDuration),
    m_pingStatus (false),
    m_debug (false)
{
}
void
DceUmipTestCase::Finished (int *pstatus, uint16_t pid, int status)
{
  *pstatus = status;
}
void
DceUmipTestCase::DoRun (void)
{
  //
  //  Step 1
  //  Node Basic Configuration
  //
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
  Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();
  x->SetAttribute ("Min", DoubleValue (0.0));
  x->SetAttribute ("Max", DoubleValue (200.0));
  r_position->SetX (10);
  r_position->SetY (50);
  r_position->SetRho (x);
  mobility.SetPositionAllocator (r_position);
  mobility.SetMobilityModel ("ns3::RandomDirection2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (0, 200, 30, 60)),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=10.0]"),
                             "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.2]"));
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

  //
  // Step 2
  // Address Configuration
  //

  DceManagerHelper dceMng;
  //      dceMng.SetLoader ("ns3::DlmLoaderFactory");
  dceMng.SetTaskManagerAttribute ("FiberManagerType",
                                  EnumValue (0));
  dceMng.SetNetworkStack ("ns3::LinuxSocketFdFactory",
                          "Library", StringValue ("liblinux.so"));
  dceMng.Install (ha);
  dceMng.Install (mr);
  dceMng.Install (ar);
  dceMng.Install (mnn);

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
  RunIp (ar.Get (1), Seconds (0.15), "-6 route add 2001:1:2:5::1/64 via 2001:1:2:3::1 dev sim0");
  //  RunIp (ar.Get (1), Seconds (0.15), "route show table all");
  kern = ar.Get (1)->GetObject<LinuxSocketFdFactory>();
  Simulator::ScheduleWithContext (ar.Get (1)->GetId (), Seconds (0.1),
                                  MakeEvent (&LinuxSocketFdFactory::Set, kern,
                                             ".net.ipv6.conf.all.forwarding", "1"));

  // For MR
  RunIp (mr.Get (0), Seconds (0.11), "link set lo up");
  RunIp (mr.Get (0), Seconds (5.11), "link set sim0 up");
  RunIp (mr.Get (0), Seconds (3.0), "link set ip6tnl0 up");
  AddAddress (mr.Get (0), Seconds (0.12), "sim1", "2001:1:2:5::1/64");
  RunIp (mr.Get (0), Seconds (0.13), "link set sim1 up");

  // For MNN
  RunIp (mnn.Get (0), Seconds (0.11), "link set lo up");
  RunIp (mnn.Get (0), Seconds (0.11), "link set sim0 up");


  if (m_debug)
    {
      Packet::EnablePrinting ();
      csma.EnablePcapAll ("dce-umip-test-" + m_testname);
      phy.EnablePcapAll ("dce-umip-test-" + m_testname);
    }

  //
  // Step 3
  // Quagga/UMIP Configuration
  //
  QuaggaHelper quagga;
  Mip6dHelper mip6d;
  ApplicationContainer apps;

  // HA
  if (m_testname == "NEMO")
    {
      mip6d.AddHaServedPrefix (ha.Get (0), Ipv6Address ("2001:1:2::"), Ipv6Prefix (48));
    }
  mip6d.EnableHA (ha);
  mip6d.Install (ha);

  // MR
  mip6d.AddMobileNetworkPrefix (mr.Get (0), Ipv6Address ("2001:1:2:5::1"), Ipv6Prefix (64));
  std::string ha_addr = ha_sim0;
  ha_addr.replace (ha_addr.find ("/"), 3, "\0  ");
  mip6d.AddHomeAgentAddress (mr.Get (0), Ipv6Address (ha_addr.c_str ()));
  mip6d.AddHomeAddress (mr.Get (0), Ipv6Address ("2001:1:2:3::1000"), Ipv6Prefix (64));
  mip6d.AddEgressInterface (mr.Get (0), "sim0");
  if (m_testname == "NEMO")
    {
      mip6d.EnableMR (mr);
    }
  mip6d.Install (mr);

  quagga.EnableRadvd (mr.Get (0), "sim1", "2001:1:2:5::/64");
  quagga.EnableZebraDebug (mr);
  quagga.Install (mr);

  // AR
  quagga.EnableRadvd (ar.Get (0), "sim0", "2001:1:2:3::/64");
  quagga.EnableHomeAgentFlag (ar.Get (0), "sim0");
  quagga.EnableHomeAgentFlag (ar.Get (1), "sim0");
  quagga.EnableRadvd (ar.Get (0), "sim1", "2001:1:2:4::/64");
  quagga.EnableRadvd (ar.Get (0), "sim2", "2001:1:2:6::/64");
  quagga.EnableRadvd (ar.Get (1), "sim1", "2001:1:2:7::/64");
  quagga.EnableZebraDebug (ar);
  quagga.Install (ar);

  //
  // Step 4
  // Set up ping application
  //
  // MNN
  // Ping6
  /* Install IPv4/IPv6 stack */
  DceApplicationHelper dce;
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

  uint32_t packetSize = 1024;
  uint32_t maxPacketCount = 50000000;
  Time interPacketInterval = Seconds (1.);

  dce.SetBinary ("ping6");
  dce.SetStackSize (1 << 16);
  dce.ResetArguments ();
  dce.ResetEnvironment ();
  // dce.AddArgument ("-i");
  // dce.AddArgument (interPacketInterval.GetSeconds ());
  std::ostringstream oss;
  oss << i2.GetAddress (0, 1);
  dce.AddArgument (oss.str ());
  if (m_testname == "MIP6")
    {
      apps = dce.Install (mr.Get (0));
      // Configure Validity Check Parser
      Config::Connect ("/NodeList/3/DeviceList/0/$ns3::WifiNetDevice/Mac/MacRx",
                       MakeCallback (&DceUmipTestCase::WifiRxCallback, this));
    }
  else if (m_testname == "NEMO")
    {
      apps = dce.Install (mnn.Get (0));
      // Configure Validity Check Parser
      Config::Connect ("/NodeList/5/DeviceList/0/$ns3::WifiNetDevice/Mac/MacRx",
                       MakeCallback (&DceUmipTestCase::WifiRxCallback, this));
    }
  apps.Start (Seconds (50.0));


  //
  // Step 4
  // Now It's ready to GO!
  //
  if (m_maxDuration.IsStrictlyPositive ())
    {
      Simulator::Stop (m_maxDuration);
    }
  Simulator::Run ();
  Simulator::Destroy ();


  //
  // Step 5
  // Vetify the test
  //
  NS_TEST_ASSERT_MSG_EQ (m_pingStatus, true, "Umip test " << m_testname
                                                          << " did not return successfully: " << g_testError);
  if (m_debug)
    {
      OUTPUT ("Umip test " << m_testname
                           << " stack done. status = " << m_pingStatus);

    }

  // XXX: needs to remove files
  if (m_debug)
    {
      ::system (("/bin/mv -f files-0 files-0-" + m_testname).c_str ());
      ::system (("/bin/mv -f files-1 files-1-" + m_testname).c_str ());
      ::system (("/bin/mv -f files-2 files-2-" + m_testname).c_str ());
      ::system (("/bin/mv -f files-3 files-3-" + m_testname).c_str ());
      ::system (("/bin/mv -f files-5 files-5-" + m_testname).c_str ());
    }
  else
    {
      ::system ("/bin/rm -rf files-*/usr/local/etc/*.pid");
    } 


}

static class DceUmipTestSuite : public TestSuite
{
public:
  DceUmipTestSuite ();
private:
} g_processTests;
//


DceUmipTestSuite::DceUmipTestSuite ()
  : TestSuite ("dce-umip", UNIT)
{
  typedef struct
  {
    const char *name;
    int duration;
  } testPair;

  const testPair tests[] = {
    { "MIP6", 300},
    { "NEMO", 300},
  };

  ::system ("/bin/rm -rf files-*/usr/local/etc/*.pid");
  for (unsigned int i = 0; i < sizeof(tests) / sizeof(testPair); i++)
    {
      AddTestCase (new DceUmipTestCase (std::string (tests[i].name),
                                        Seconds (tests[i].duration)));
    }
}

} // namespace ns3

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

// ADDED:
#include "ns3/olsr-helper.h"
#include "ns3/ipv4-list-routing-helper.h"

using namespace ns3;

int main (int argc, char *argv[])
{
  // parametroi via cmd
  std::string delay = "2ms";
  double failDownAt = 2.0;
  double failUpAt   = 3.0;

  CommandLine cmd;
  cmd.AddValue("delay", "Link delay (e.g., 1ms, 5ms, 10ms)", delay);
  cmd.AddValue("failDownAt", "Time (s) link 0-1 goes DOWN", failDownAt);
  cmd.AddValue("failUpAt",   "Time (s) link 0-1 goes UP",   failUpAt);
  cmd.Parse(argc, argv);

  // nodes
  NodeContainer nodes; nodes.Create(5); // 0..4

  // ADDED: Use OLSR as the routing protocol
  OlsrHelper olsr;
  Ipv4ListRoutingHelper list;
  list.Add(olsr, 10);

  InternetStackHelper stack;
  stack.SetRoutingHelper(list); // must be set before Install()
  stack.Install(nodes);

  // links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay",   StringValue(delay));

  NodeContainer n01(nodes.Get(0), nodes.Get(1));
  NodeContainer n12(nodes.Get(1), nodes.Get(2));
  NodeContainer n03(nodes.Get(0), nodes.Get(3));
  NodeContainer n34(nodes.Get(3), nodes.Get(4));
  NodeContainer n42(nodes.Get(4), nodes.Get(2));

  NetDeviceContainer d01 = p2p.Install(n01);
  NetDeviceContainer d12 = p2p.Install(n12);
  NetDeviceContainer d03 = p2p.Install(n03);
  NetDeviceContainer d34 = p2p.Install(n34);
  NetDeviceContainer d42 = p2p.Install(n42);

  // addressing
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i01 = address.Assign(d01);
  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i12 = address.Assign(d12);
  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i03 = address.Assign(d03);
  address.SetBase("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i34 = address.Assign(d34);
  address.SetBase("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer i42 = address.Assign(d42);

  // REMOVED: Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // UDP Echo: server on node 2, client on node 0
  int port = 9;
  UdpEchoServerHelper echoServer(port);
  auto serverApps = echoServer.Install(nodes.Get(2));
  serverApps.Start(Seconds(0.5));
  serverApps.Stop(Seconds(10.0));

  UdpEchoClientHelper echoClient(i12.GetAddress(1), port); // node2 addr on 1–2
  echoClient.SetAttribute("MaxPackets", UintegerValue(0));
  echoClient.SetAttribute("Interval",   TimeValue(Seconds(0.02)));
  echoClient.SetAttribute("PacketSize", UintegerValue(128));
  auto clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(10.0));

  // link failure (0–1)
  Ptr<NetDevice> dev0 = d01.Get(0);
  Ptr<NetDevice> dev1 = d01.Get(1);
  Ptr<Ipv4> ipv4Node0 = nodes.Get(0)->GetObject<Ipv4>();
  Ptr<Ipv4> ipv4Node1 = nodes.Get(1)->GetObject<Ipv4>();
  int ifIndex0 = ipv4Node0->GetInterfaceForDevice(dev0);
  int ifIndex1 = ipv4Node1->GetInterfaceForDevice(dev1);

  if (ifIndex0 == -1 || ifIndex1 == -1) {
    std::cerr << "ERROR: interface index not found for Node0/Node1\n";
  } else {
    Simulator::Schedule(Seconds(failDownAt), &Ipv4::SetDown, ipv4Node0, (int)ifIndex0);
    Simulator::Schedule(Seconds(failDownAt), &Ipv4::SetDown, ipv4Node1, (int)ifIndex1);

    // REMOVED: PopulateRoutingTables() reschedules — OLSR handles reconvergence

    Simulator::Schedule(Seconds(failUpAt), &Ipv4::SetUp, ipv4Node0, (int)ifIndex0);
    Simulator::Schedule(Seconds(failUpAt), &Ipv4::SetUp, ipv4Node1, (int)ifIndex1);
  }

  // FlowMonitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  struct Acc { int tx=0, rx=0, lost=0, rxBytes=0; double delay=0; double t=0; } last;

  auto Snapshot = [&](const char* label){
    monitor->CheckForLostPackets();
    const auto &stats = monitor->GetFlowStats();

    int tx=0, rx=0, lost=0, rxB=0; double dsum=0;
    for (const auto &kv: stats) {
      const auto &s = kv.second;
      tx   += s.txPackets;
      rx   += s.rxPackets;
      lost += s.lostPackets;
      rxB  += s.rxBytes;
      dsum += s.delaySum.GetSeconds();
    }

    double now = Simulator::Now().GetSeconds();
    double dt  = std::max(1e-9, now - last.t);

    int dTx = tx - last.tx;
    int dRx = rx - last.rx;
    int dLost = lost - last.lost;
    int dRxB = rxB - last.rxBytes;
    double dDelay = dsum - last.delay;

    double thrMbps = (dRxB * 8.0) / (dt * 1e6);
    double pdrPct  = (dTx > 0) ? (100.0 * (double)dRx / (double)dTx) : 0.0;
    double avgMs   = (dRx > 0) ? (dDelay / (double)dRx) * 1000.0 : 0.0;

    std::cout.setf(std::ios::fixed);
    std::cout.precision(3);
    std::cout << now << "s (" << label << ")\n"
              << "   Throughput: " << thrMbps << " Mbps\n"
              << "   Packet Delivery Ratio: " << pdrPct << " %\n"
              << "   Average Delay: " << avgMs << " ms\n"
              << "   Packets Sent: " << dTx
              << ", Received: " << dRx
              << ", Lost: " << dLost << "\n"
              << "----------------------------------------\n";

    last = {tx, rx, lost, rxB, dsum, now};
  };

  // schedule snapshots
  Simulator::Schedule(Seconds(std::max(1.0, failDownAt - 0.2)), [=,&Snapshot](){
    Snapshot("pre-failure");
  });
  Simulator::Schedule(Seconds(failDownAt + 0.4), [=,&Snapshot](){
    Snapshot("during-failure");
  });
  Simulator::Schedule(Seconds(failUpAt + 0.4), [=,&Snapshot](){
    Snapshot("post-recovery");
  });

  // run
  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}







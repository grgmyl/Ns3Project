#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main (int argc, char *argv[])
{
  
  // dataRate  -> Transmission speed of the point-to-point link: "5Mbps"
  // delay     
  // pktSize   -> UDP packet size in bytes: 1024
  // maxPkts   -> Number of packets each client sends: 5
  // simTime   -> Total simulation time (seconds)

  std::string dataRate = "5Mbps";
  std::string delay = "6ms";
  uint32_t pktSize = 1024; //Unsigned 32 bit Intiger
  uint32_t maxPkts = 6;
  double simTime = 11.0;

  // Creating 2  nodes
  NodeContainer nodes;
  nodes.Create(2); // Node 0 -> Clients, Node 1 -> Servers

  //Point-to-Point connection 
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue(dataRate));
  p2p.SetChannelAttribute("Delay", StringValue(delay));
  NetDeviceContainer devices = p2p.Install(nodes);

  // Internet + IP 
  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifaces = address.Assign(devices);
  Ipv4Address serverAddr = ifaces.GetAddress(1);

  // (x2) UDP Echo Servers for node 1
  uint16_t port1 = 9, port2 = 10;
  UdpEchoServerHelper server1(port1);
  UdpEchoServerHelper server2(port2);
  ApplicationContainer s1 = server1.Install(nodes.Get(1));
  ApplicationContainer s2 = server2.Install(nodes.Get(1));
  s1.Start(Seconds(0.5)); s1.Stop(Seconds(simTime - 1.0));
  s2.Start(Seconds(0.5)); s2.Stop(Seconds(simTime - 1.0));

  // (x2) UDP Echo Clients for node 0 
  UdpEchoClientHelper client1(serverAddr, port1);
  client1.SetAttribute("MaxPackets", UintegerValue(maxPkts));
  client1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  client1.SetAttribute("PacketSize", UintegerValue(pktSize));

  UdpEchoClientHelper client2(serverAddr, port2);
  client2.SetAttribute("MaxPackets", UintegerValue(maxPkts));
  client2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  client2.SetAttribute("PacketSize", UintegerValue(pktSize));

  ApplicationContainer c1 = client1.Install(nodes.Get(0));
  ApplicationContainer c2 = client2.Install(nodes.Get(0));
  c1.Start(Seconds(1.0));  c1.Stop(Seconds(simTime - 2.0));
  c2.Start(Seconds(1.5));  c2.Stop(Seconds(simTime - 1.5));

  //FlowMonitor to monitor trafic
  FlowMonitorHelper fmHelper;
  Ptr<FlowMonitor> monitor = fmHelper.InstallAll();

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();


  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(fmHelper.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  std::cout << "\nResults of Flows \n";

  for (const auto &kv : stats)
  {
    FlowId id = kv.first;
    const FlowMonitor::FlowStats &st = kv.second;
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(id);

    double duration = (st.timeLastRxPacket - st.timeFirstTxPacket).GetSeconds();
    double throughputMbps = (duration > 0) ? (st.rxBytes * 8.0 / duration) / 1e6 : 0.0;
    double meanDelayMs = (st.rxPackets > 0) ? (st.delaySum.GetSeconds() / st.rxPackets) * 1000.0 : 0.0;

    std::cout << "\nID of Flow: " << id << "\n";
    std::cout << "From : " << t.sourceAddress << ":" << t.sourcePort
              << "  ->  To: " << t.destinationAddress << ":" << t.destinationPort << "\n";

    std::cout << "  Packets Send: " << st.txPackets << "\n";
    std::cout << "  Packets Received: " << st.rxPackets << "\n";
    std::cout << "  Packets Lost: " << st.lostPackets << "\n";
    std::cout << "  Average Delay: " << meanDelayMs << " ms\n";
    std::cout << "  Bandwidth: " << throughputMbps << " Mbps\n";
  }

 
  Simulator::Destroy();
  return 0;
}


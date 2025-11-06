// fine-node-topology.cc

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main (int argc, char *argv[])
{
  // 1) Παράμετρος καθυστέρησης
  std::string delay = "2ms";
  CommandLine cmd;
  cmd.AddValue("delay", "Καθυστέρηση για όλα τα links (π.χ. 1ms, 5ms, 10ms)", delay);
  cmd.Parse(argc, argv);

  //  Εκτύπωση για επιβεβαίωση
  std::cout << "-------------------------------" << std::endl;
  std::cout << "Καθυστέρηση links: " << delay << std::endl;
  std::cout << "-------------------------------" << std::endl;

  // 2) Κόμβοι
  NodeContainer nodes;
  nodes.Create(5); // 0..4

  // 3) Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // 4) Point-to-point links
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue(delay));

  // 5) Συνδέσεις
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

  // 6) IP διευθύνσεις
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

  // 7) Δρομολόγηση
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // 8) UDP Echo: server στον Node 2, client στον Node 0
  uint16_t port = 9;
  UdpEchoServerHelper echoServer(port);
  ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
  serverApps.Start(Seconds(0.5));
  serverApps.Stop(Seconds(10.0));

  Ipv4Address dst = i12.GetAddress(1); // IP του Node2 στο link 1–2
  UdpEchoClientHelper echoClient(dst, port);
  echoClient.SetAttribute("MaxPackets", UintegerValue(10));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(64));

  ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(10.0));

    // 9) PCAP για ανάλυση
  p2p.EnablePcapAll("five-node-topology");

  // 10) Συμβάντα αποτυχίας και επαναφοράς σύνδεσης (Node0–Node1)
// --- Prepare to bring link 0-1 down and up at specific times ---

// Get the two NetDevices on the 0-1 link
//Ptr<NetDevice> dev0 = d01.Get(0); // device on Node0
//Ptr<NetDevice> dev1 = d01.Get(1); // device on Node1
////////////////////////////////////////////////////////



// Get the Ipv4 objects for node 0 and node 1
Ptr<Ipv4> ipv4Node0 = nodes.Get(0)->GetObject<Ipv4>();
Ptr<Ipv4> ipv4Node1 = nodes.Get(1)->GetObject<Ipv4>();

// Find the interface index corresponding to each NetDevice
int32_t ifIndex0 = ipv4Node0->GetInterfaceForDevice(dev0);
int32_t ifIndex1 = ipv4Node1->GetInterfaceForDevice(dev1);

// Safety check: ensure interface indices were found
if (ifIndex0 == -1 || ifIndex1 == -1) {
  std::cerr << "ERROR: Could not find interface index for device on Node0 or Node1" << std::endl;
} else {
  // Schedule interface down at t = 2.0 s
  Simulator::Schedule(Seconds(2.0), &Ipv4::SetDown, ipv4Node0, (uint32_t) ifIndex0);
  Simulator::Schedule(Seconds(2.0), &Ipv4::SetDown, ipv4Node1, (uint32_t) ifIndex1);

  // Optional: print when link goes down
  Simulator::Schedule(Seconds(2.0), [](){
    std::cout << "*** Link 0–1 DOWN at t=" << Simulator::Now().GetSeconds() << "s ***" << std::endl;
  });

  // Recompute global routing shortly after bringing interface down so routes adapt
  Simulator::Schedule(Seconds(2.01), &Ipv4GlobalRoutingHelper::PopulateRoutingTables);

  // Schedule interface up at t = 3.0 s
  Simulator::Schedule(Seconds(3.0), &Ipv4::SetUp, ipv4Node0, (uint32_t) ifIndex0);
  Simulator::Schedule(Seconds(3.0), &Ipv4::SetUp, ipv4Node1, (uint32_t) ifIndex1);

  // Optional: print when link goes up
  Simulator::Schedule(Seconds(3.0), [](){
    std::cout << "*** Link 0–1 UP at t=" << Simulator::Now().GetSeconds() << "s ***" << std::endl;
  });

  // Recompute routing again after restoration
  Simulator::Schedule(Seconds(3.01), &Ipv4GlobalRoutingHelper::PopulateRoutingTables);
}
////////////////////////////////



  // Προαιρετικά: μηνύματα στο terminal
  Simulator::Schedule(Seconds(2.0), [](){
    std::cout << "*** Link 0–1 DOWN ***" << std::endl;
  });
  Simulator::Schedule(Seconds(3.0), [](){
    std::cout << "*** Link 0–1 UP ***" << std::endl;
  });

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}
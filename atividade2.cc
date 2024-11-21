#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include <random>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LinearTopologySimulation");

// Função para gerar números aleatórios
int GenerateRandomNumber() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 100);
    return dis(gen);
}

// Função de envio de dados
void SendData(Ptr<Socket> socket, Address destAddress, int data) {
    if (!socket) {
        NS_LOG_ERROR("Socket inválido! Envio abortado.");
        return;
    }
    NS_LOG_INFO("Enviando dado: " << data << " para " << InetSocketAddress::ConvertFrom(destAddress).GetIpv4());
    Ptr<Packet> packet = Create<Packet>((uint8_t *)&data, sizeof(data));
    socket->SendTo(packet, 0, destAddress);
}

// Função de recepção de dados
void ReceiveData(Ptr<Socket> socket) {
    NS_LOG_INFO("Recepção de dados iniciada no Nó " << socket->GetNode()->GetId());
    Address from;
    Ptr<Packet> packet = socket->RecvFrom(from);

    if (!packet) {
        NS_LOG_ERROR("Erro ao receber o pacote!");
        return;
    }

    int receivedData;
    packet->CopyData((uint8_t *)&receivedData, sizeof(receivedData));

    Ptr<Node> node = socket->GetNode();
    uint32_t nodeId = node->GetId();
    NS_LOG_INFO("Nó " << nodeId << " recebeu valor: " << receivedData << " de " << InetSocketAddress::ConvertFrom(from).GetIpv4());

    // Determinar o próximo nó e encaminhar
    Address nextAddress;
    int newData;

    if (nodeId == 1) {
        if (InetSocketAddress::ConvertFrom(from).GetIpv4() == Ipv4Address("10.0.0.1")) {
            NS_LOG_INFO("Nó 1 recebeu de N0. Encaminhando para N2.");
            nextAddress = InetSocketAddress(Ipv4Address("10.0.0.3"), 8002);
            SendData(socket, nextAddress, receivedData);
        } else {
            NS_LOG_INFO("Nó 1 recebeu de N2. Gerando novo valor e encaminhando para N2.");
            newData = GenerateRandomNumber();
            nextAddress = InetSocketAddress(Ipv4Address("10.0.0.3"), 8002);
            SendData(socket, nextAddress, newData);
        }
    } else if (nodeId == 4) {
        newData = GenerateRandomNumber();
        NS_LOG_INFO("Nó 4 gerou novo valor: " << newData << " e está encaminhando para N3.");
        nextAddress = InetSocketAddress(Ipv4Address("10.0.0.4"), 8003);
        SendData(socket, nextAddress, newData);
    } else if (nodeId == 2 || nodeId == 3) {
        if (nodeId == 2) {
            NS_LOG_INFO("Nó 2 recebeu de "
                        << (InetSocketAddress::ConvertFrom(from).GetIpv4() == Ipv4Address("10.0.0.2") ? "N1" : "N3")
                        << ", encaminhando para "
                        << (InetSocketAddress::ConvertFrom(from).GetIpv4() == Ipv4Address("10.0.0.2") ? "N3" : "N1") << ".");
            nextAddress = InetSocketAddress(
                (InetSocketAddress::ConvertFrom(from).GetIpv4() == Ipv4Address("10.0.0.2"))
                    ? Ipv4Address("10.0.0.4")
                    : Ipv4Address("10.0.0.2"),
                (InetSocketAddress::ConvertFrom(from).GetIpv4() == Ipv4Address("10.0.0.2")) ? 8003 : 8001);
        } else {
            NS_LOG_INFO("Nó 3 recebeu de "
                        << (InetSocketAddress::ConvertFrom(from).GetIpv4() == Ipv4Address("10.0.0.4") ? "N4" : "N2")
                        << ", encaminhando para "
                        << (InetSocketAddress::ConvertFrom(from).GetIpv4() == Ipv4Address("10.0.0.4") ? "N2" : "N4") << ".");
            nextAddress = InetSocketAddress(
                (InetSocketAddress::ConvertFrom(from).GetIpv4() == Ipv4Address("10.0.0.4"))
                    ? Ipv4Address("10.0.0.3")
                    : Ipv4Address("10.0.0.5"),
                (InetSocketAddress::ConvertFrom(from).GetIpv4() == Ipv4Address("10.0.0.4")) ? 8002 : 8004);
        }
        SendData(socket, nextAddress, receivedData);
    }
}

// Callback para aceitar conexões e configurar o receptor
void AcceptConnection(Ptr<Socket> socket, const Address &from) {
    if (!socket) {
        NS_LOG_ERROR("Socket inválido na aceitação de conexão!");
        return;
    }
    NS_LOG_INFO("Conexão aceita por Nó " << socket->GetNode()->GetId()
                                          << " de " << InetSocketAddress::ConvertFrom(from).GetIpv4());
    socket->SetRecvCallback(MakeCallback(&ReceiveData));
}

// Configuração do socket em cada nó
void ConfigureSocket(Ptr<Node> node, Address localAddress) {
    Ptr<Socket> recvSocket = Socket::CreateSocket(node, TcpSocketFactory::GetTypeId());
    recvSocket->Bind(localAddress);
    recvSocket->Listen();
    recvSocket->SetAcceptCallback(
        MakeNullCallback<bool, Ptr<Socket>, const Address &>(),
        MakeCallback(&AcceptConnection));

    NS_LOG_INFO("Nó " << node->GetId() << " está ouvindo em "
                      << InetSocketAddress::ConvertFrom(localAddress).GetIpv4()
                      << ":" << InetSocketAddress::ConvertFrom(localAddress).GetPort());
}

int main(int argc, char *argv[]) {
    LogComponentEnable("LinearTopologySimulation", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(5);

    // Configuração de WiFi
    WifiHelper wifi;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Mobilidade fixa
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Instalar pilha TCP/IPv4
    InternetStackHelper stack;
    stack.Install(nodes);

    // Configurar endereços IP
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Configurar sockets para cada nó
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Address localAddress = InetSocketAddress(interfaces.GetAddress(i), 8000 + i);
        ConfigureSocket(nodes.Get(i), localAddress);
    }

    // Enviar dado inicial do N0
    Ptr<Socket> initialSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
    Simulator::Schedule(Seconds(1.0), &SendData, initialSocket, InetSocketAddress(Ipv4Address("10.0.0.2"), 8001), GenerateRandomNumber());

    Simulator::Stop(Seconds(30.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

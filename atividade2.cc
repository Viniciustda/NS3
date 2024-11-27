/*
A presente atividade consiste na criação e execução de um cenário de redes no NS3. Este cenário deve atender as seguintes especificações:

    1. Implementar uma topologia em barra com 5 nós (os nós não devem mudar de posição)

                  N0 ------- N1 ------- N2 ------- N3 ------- N4

    2. Configurar um canal de comunicação sem fio entre os nós
    3. Utilize o protocolo de internet (TCP/IPv4), onde o endereço da rede deve ser 10.0.0.0 e mascara 255.0.0.0
    4. Desenvolva uma aplicação simples:
        a. Cada nó deve ter dois sockets (um para enviar dados e outro para ouvir)
        b. O nó N0 deve enviar (pelo socket) um valor inteiro aleatório (entre 0 e 100) apenas para seu vizinho N1, este por sua vez deve imprimir o valor recebido no terminal e encaminhar o dado para N2. O procedimento deve ser repetido até alcançar N4, onde N4 deve imprimir o valor recebido no terminal, gerar um novo valor aleatório (entre 0 e 100) e enviar para o seu vizinho N3, este por sua vez deve imprimir o valor recebido no terminal e encaminhar o dado para N2. O procedimento deve ser repetido até alcançar N1, onde N1 deve imprimir o valor recebido no terminal, gerar um novo valor aleatório (entre 0 e 100) e repetir o mesmo procedimento.
    5. O tempo total da simulação deve ser de 30 segundos.
*/

#include "ns3/core-module.h"             // Módulo central do NS-3 (simulação de eventos, logs, etc.)
#include "ns3/network-module.h"          // Módulo para abstrações de rede
#include "ns3/internet-module.h"         // Módulo para pilha de protocolos TCP/IP
#include "ns3/wifi-module.h"             // Módulo para simulações WiFi
#include "ns3/mobility-module.h"         // Módulo para configurar mobilidade dos nós
#include "ns3/applications-module.h"     // Módulo para criar aplicações na simulação
#include <netinet/in.h>                  // Biblioteca padrão para conversão de ordem de bytes
#include <random>                        // Biblioteca para geração de números aleatórios

using namespace ns3;
#define NUM_NODES 5                      // Define o número de nós na simulação

NS_LOG_COMPONENT_DEFINE("Atividade2");   // Define o componente de log para "Atividade2"

/*
    Fluxo de comunicaçao:

    NO -> N1 -> N2 -> N3 -> N4
    N4 -> N3 -> N2 -> N1
    N1 -> N2 -> N3 -> N4
    N4 -> N3 -> N2 -> N1
    N1 -> N2 -> N3 -> N4
    ...
    
    Ou seja:

    N0:
        Gera um número aleatório e envia para N1.
        Após o envio inicial, não faz mais nada.

    N1:
        Recebe o valor.
        Sempre imprime o valor recebido.
        Se o valor veio de N0, encaminha para N2.
        Se o valor veio de N2, gera um novo número e envia para N2.

    N2:
    	Sempre imprime o valor recebido.
    	Sempre encaminham o valor recebido:
            De N1 para N3 (se veio do menor).
            De N3 para N1 (se veio do maior).
    N3:
    	Sempre imprime o valor recebido.
        Sempre encaminham o valor recebido:
            De N2 para N4 (se veio do menor).
            De N4 para N2 (se veio do maior).

    N4:
        Recebe o valor.
        Sempre imprime o valor recebido.
        Gera um novo número e envia para N3.

    Duração: 30s
 */

// Função para gerar números aleatórios
int GenerateRandomValue() {

    static std::random_device rd;                       // Semente para o gerador de números aleatórios
    static std::mt19937 gen(rd());                      // Gerador Mersenne Twister
    static std::uniform_int_distribution<> dis(0, 100); // Distribuição uniforme no intervalo [0, 100]

    return dis(gen);                                   // Retorna um número aleatório
}

// Classe TcpApp: representa a aplicação para cada nó na rede TCP
class TcpApp : public Application {

    public:

        TcpApp();                                         // Construtor
        virtual ~TcpApp();                                // Destrutor

        static TypeId GetTypeId (void);                  // Retorna o TypeId da aplicação
        void ConfigureApplication (int id,Ptr<Node> node,Ptr<Socket> sender_socket,Ptr<Socket> receiver_socket,Ipv4Address right_neighbor_ip,Ipv4Address left_neighbor_ip,bool generator);

        void StartApplication() override;                // Sobrescreve a inicialização da aplicação
        void StopApplication() override;                 // Sobrescreve o encerramento da aplicação

        // Callbacks para conexões e recepção de pacotes
        void HandleConnectionAccept (Ptr<Socket> s, const Address& from);
        void ProcessReceivedPacket (Ptr<Socket> socket);
        void EstablishNeighborLink (Ipv4Address neighbor_address);
        void ConnectionSucceeded(Ptr<Socket> socket);
        void ConnectionFailed(Ptr<Socket> socket);
        bool ValidateConnection(Ptr<Socket> socket, const Address& from);

        void SendPacket (int32_t number);               // Envia pacotes para um vizinho

        // Variaveis
        int id;                                         // Índice do nó
        Ptr<Node> node;                                 // Nó associado à aplicação
        Ptr<Socket> sender_socket;                      // Socket para enviar pacotes
        Ptr<Socket> receiver_socket;                    // Socket para receber pacotes
        uint16_t port = 8080;                           // Porta de recepção
        bool generator;                                 // Indica se o nó é gerador de número
        Ipv4Address right_neighbor_ip;                  // Endereço IP do vizinho direito
        Ipv4Address left_neighbor_ip;                   // Endereço IP do vizinho esquerdo
};

// Construtor da aplicação
TcpApp::TcpApp() {
    sender_socket = 0;
    receiver_socket = 0;
    generator = false;
}

// Destrutor da aplicação
TcpApp::~TcpApp() {
    sender_socket = 0;
    receiver_socket = 0;
}

/// Registra o TypeId da aplicação
TypeId TcpApp::GetTypeId(void) {

    static TypeId tid = TypeId("TcpApp")
        .SetParent<Application>()      // Define como uma subclasse de Application
        .AddConstructor<TcpApp>();     // Permite a criação de objetos da classe
    return tid;
}

// Configuração inicial da aplicação
void TcpApp::ConfigureApplication(int id,Ptr<Node> node,Ptr<Socket> sender_socket,Ptr<Socket> receiver_socket,Ipv4Address right_neighbor_ip,Ipv4Address left_neighbor_ip,bool generator = false) {
    
    this->id = id;
    this->node = node;
    this->sender_socket = sender_socket;
    this->receiver_socket = receiver_socket;
    this->right_neighbor_ip = right_neighbor_ip;
    this->left_neighbor_ip = left_neighbor_ip;
    this->generator = generator;
}

// Método chamado ao iniciar a aplicação
void TcpApp::StartApplication(void) {

    // Criação de sockets para envio e recepção de pacotes
    Ptr<Socket> receiver_socket = Socket::CreateSocket (this->node, TcpSocketFactory::GetTypeId ());
    Ptr<Socket> sender_socket = Socket::CreateSocket (this->node, TcpSocketFactory::GetTypeId ());

    // Configuração do socket receptor
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port);
    if (receiver_socket->Bind(local) == -1) {
      NS_FATAL_ERROR("Failed to bind socket");
    }
    receiver_socket->Listen();
    receiver_socket->SetAcceptCallback(
      MakeCallback(&TcpApp::ValidateConnection, this),
      MakeCallback(&TcpApp::HandleConnectionAccept, this)
    );

    this->receiver_socket = receiver_socket;
    this->sender_socket = sender_socket;

    // O primeiro nó gera e envia o primeiro número
    if (this->id == 0) {
        int32_t number =  GenerateRandomValue();
        EstablishNeighborLink(this->left_neighbor_ip);
        SendPacket(number);
    }
}

// Método chamado ao encerrar a aplicação
void TcpApp::StopApplication(void) {

    if (this->receiver_socket) {
        this->receiver_socket->Close();
        this->receiver_socket = nullptr;
    }

    if (this->sender_socket) {
        this->sender_socket->Close();
        this->sender_socket = nullptr;
    }
    NS_LOG_UNCOND("Aplicação encerrada");
}

// Callback chamado quando uma conexão é aceita
void TcpApp::HandleConnectionAccept(Ptr<Socket> s, const Address& from) {
    s->SetRecvCallback(MakeCallback(&TcpApp::ProcessReceivedPacket, this));
}

// Callback chamado ao receber um pacote
void TcpApp::ProcessReceivedPacket(Ptr<Socket> socket) {

    Address from;                        // Endereço do remetente do pacote
    Ptr<Packet> packet;                  // Ponteiro para o pacote recebido
    int32_t networkOrderNumber;          // Número no formato de ordem de rede
    int32_t receivedNumber = 0;          // Número recebido (convertido para ordem do host)

    // Cria um novo socket para envio, reutilizável nas operações de resposta
    Ptr<Socket> sender_socket = Socket::CreateSocket(this->node, TcpSocketFactory::GetTypeId());
    this->sender_socket = sender_socket;

    // Loop para processar todos os pacotes recebidos
    while ((packet = socket->RecvFrom(from))) {
        if (packet->GetSize() == 0) {     // Se o pacote estiver vazio, interrompe o loop
            break;
        }

        // Converte o endereço do remetente para InetSocketAddress para obter o IP
        InetSocketAddress inetFrom = InetSocketAddress::ConvertFrom(from);

        // Copia os dados do pacote para a variável networkOrderNumber
        packet->CopyData((uint8_t *)&networkOrderNumber, sizeof(networkOrderNumber));
        receivedNumber = ntohl(networkOrderNumber); // Converte o número para ordem do host

        // Exibe o número recebido no log
        NS_LOG_UNCOND("no: " << this->id << " recebeu: " << receivedNumber);

        // Verifica condições específicas para o nó 1. N1 passa a gerar pacote e envia para N2, N0 nao participa mais da simulacao
        if (this->id == 1 && inetFrom.GetIpv4() == "10.0.0.1") {
            this->left_neighbor_ip = this->right_neighbor_ip;            // Atualiza o vizinho esquerdo
            this->generator = true;                                      // Define o nó como extremidade
            EstablishNeighborLink(this->right_neighbor_ip);              // Conecta ao próximo nó
            SendPacket(receivedNumber);                                  // Envia o pacote recebido
            continue;                                                    // Continua para o próximo pacote
        }

        // Se o nó for uma extremidade, gera um novo número aleatório
        if (this->generator) {
            receivedNumber = GenerateRandomValue();
            EstablishNeighborLink(this->left_neighbor_ip);  // Conecta ao vizinho esquerdo
        } else {
            // Se o pacote veio do vizinho direito, conecta ao vizinho esquerdo
            if (this->right_neighbor_ip == inetFrom.GetIpv4()) {
                EstablishNeighborLink(this->left_neighbor_ip);
            } else { // Caso contrário, conecta ao vizinho direito
                EstablishNeighborLink(this->right_neighbor_ip);
            }
        }

        // Envia o número para o próximo nó
        SendPacket(receivedNumber);
    }
}

// Conecta a um nó vizinho
void TcpApp::EstablishNeighborLink(Ipv4Address neighbor_address) {

    this->sender_socket->SetConnectCallback (
        MakeCallback(&TcpApp::ConnectionSucceeded, this),
        MakeCallback(&TcpApp::ConnectionFailed, this)
    );
    InetSocketAddress remote = InetSocketAddress(neighbor_address, this->port);
    this->sender_socket->Connect(remote);
    NS_LOG_INFO("nó "<< this->id << " conecta com " << neighbor_address);
}

// Callback para conexão bem-sucedida
void TcpApp::ConnectionSucceeded(Ptr<Socket> socket) {
    NS_LOG_INFO("Conexão bem-sucedida");
}

// Callback para falha de conexão
void TcpApp::ConnectionFailed(Ptr<Socket> socket) {
    NS_LOG_INFO("Falha na conexão");
}

// Callback para solicitações de conexão
bool TcpApp::ValidateConnection(Ptr<Socket> socket, const Address& from) {
    NS_LOG_INFO("Conexão solicitada de: " << from);
    return true;
}

// Envia um pacote com o número fornecido
void TcpApp::SendPacket(int32_t number) {
    
    int32_t networkOrderNumber = htonl(number);
    Ptr<Packet> packet = Create<Packet>((uint8_t *)&networkOrderNumber, sizeof(networkOrderNumber));
    this->sender_socket->Send(packet);
    sender_socket->Close();
    NS_LOG_INFO("nó "<< this->id << " envia " << number);
}

int main(int argc, char *argv[]) {

    //LogComponentEnable("Atividade2", LOG_LEVEL_INFO);  // Habilita NS_LOG_INFO para "Atividade2"
    
    // Cria nós
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
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(NUM_NODES),
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
    for (int i = 0; i < NUM_NODES; i++) {
        Ptr<TcpApp> application = CreateObject<TcpApp>();
        if (i == 0) {
            // Configuração para o nó 0
            application->ConfigureApplication(i, nodes.Get(i), nullptr, nullptr, interfaces.GetAddress(i + 1), interfaces.GetAddress(i + 1), true);
        } else if (i == NUM_NODES - 1) {
            // Configuração para o último nó
            application->ConfigureApplication(i, nodes.Get(i), nullptr, nullptr, interfaces.GetAddress(i - 1), interfaces.GetAddress(i - 1), true);
        } else {
            // Configuração para os nós intermediários
            application->ConfigureApplication(i, nodes.Get(i), nullptr, nullptr, interfaces.GetAddress(i + 1), interfaces.GetAddress(i - 1), false);
        }
        application->SetStartTime(Seconds(1.));
        application->SetStopTime(Seconds(30));
        nodes.Get(i)->AddApplication(application);
    }

    Simulator::Stop(Seconds(30));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
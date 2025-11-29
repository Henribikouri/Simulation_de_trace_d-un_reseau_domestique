// *************** CODE SOURCE DE BIKOURI HENRI **********************

//************* Mon site web : henribikouri.github.io *************************
//*********************Email : henri.bikouri@enspy-uy1.cm ****************************
#include "ns3/network-module.h"
#include "ns3/internet-module.h" 
#include "ns3/applications-module.h"
#include "ns3/node-list.h"
#include "ns3/wifi-module.h" 
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-address.h"
#include "ns3/inet-socket-address.h"
#include "ns3/wifi-standards.h" 
#include "ns3/random-variable-stream.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/command-line.h"
#include "ns3/packet-sink.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <map>
#include <set>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>

using namespace ns3;


NS_LOG_COMPONENT_DEFINE ("SimulationDomestique");

// --- Choix de mes Constantes pour ce Scénario ---

// Nbre total d'équipements clients Wi-Fi
const uint32_t N_EQUIPMENTS = 32; 

// Nbre de types d'équipements/applications
const uint32_t K_TYPES = 10; 

// Durée de la simulation (en secondes)
// Par défaut cette constante vaut 600 secondes. Elle peut être modifiée par la ligne de commande (--duration)
double DUREE_SIMULATION = 600.0; // 10 minutes par défaut

// structure globale permet de suivre les récepteurs installés sur un nœud/port spécifique afin de ne pas avoir à le faire.
static std::map<uint32_t, std::map<uint16_t, Ptr<PacketSink>>> g_installedSinks;

// Fonction pour obtenir la première adresse IPv4 non locale d'un nœud.
Ipv4Address GetFirstIpv4Address(Ptr<Node> node)
{
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    if (ipv4 == nullptr) {
        return Ipv4Address("0.0.0.0");
    }
    for (uint32_t i = 0; i < ipv4->GetNInterfaces(); ++i)
    {
        for (uint32_t j = 0; j < ipv4->GetNAddresses(i); ++j)
        {
            Ipv4Address addr = ipv4->GetAddress(i, j).GetLocal();
                    
            if ((addr != Ipv4Address("0.0.0.0")) && (addr != Ipv4Address("127.0.0.1")))
            {
                return addr;
            }
        }
    }
    return Ipv4Address("0.0.0.0");
}

Ptr<PacketSink> InstallSinkIfNeeded(Ptr<Node> node, InetSocketAddress sinkSocket, const std::string &factory)
{
    uint32_t id = node->GetId();
    uint16_t port = sinkSocket.GetPort();
    auto &mapRef = g_installedSinks[id];
    
    if (mapRef.find(port) == mapRef.end())
    {
        PacketSinkHelper sinkHelper(factory, sinkSocket);
        NS_LOG_INFO ("Installation d'un récepteur sur le nœud " << id << " -> " << sinkSocket.GetIpv4() << ":" << port);
        ApplicationContainer apps = sinkHelper.Install(node);
        
        // Récupérer le pointeur d'application du récepteur réel
        Ptr<PacketSink> sink = DynamicCast<PacketSink>(apps.Get(0));
        
        mapRef[port] = sink;
        
        // Démarrer l'application d'évier
        apps.Start(Seconds(0.0));
        apps.Stop(Seconds(DUREE_SIMULATION));
        
        return sink;
    }
    else
    {
        NS_LOG_INFO ("Récepteur déjà installé sur le nœud " << id << " port " << port << "; réutilisation du récepteur existant.");
        return mapRef[port];
    }
}

// Structure globale pour suivre les sources de trafic (permet de récupérer Total Sent Packets)
struct TrafficSourceInfo {
    std::string type;
    Ptr<Application> app;
};
static std::vector<TrafficSourceInfo> g_trafficSources;



// Déclaration du générateur aléatoire pour l'heure de début
Ptr<UniformRandomVariable> debutAleatoire = CreateObject<UniformRandomVariable> ();

// --- Fonctions de Configuration des Applications (avec enregistrement des sources) ---

// 1. Caméra (Type 1, Port 9001)
void ConfigureCamera(Ptr<Node> clientNode, Ptr<Node> serverNode, double startTime)
{
    Ipv4Address remoteIp = GetFirstIpv4Address(serverNode);
    InetSocketAddress remoteSocket(remoteIp, 9001);

    UdpClientHelper clientHelper(remoteSocket);
    
    clientHelper.SetAttribute("MaxPackets", UintegerValue(100000));
    clientHelper.SetAttribute("Interval", TimeValue(MilliSeconds(50))); 
    clientHelper.SetAttribute("PacketSize", UintegerValue(1200));

    ApplicationContainer clientApp = clientHelper.Install(clientNode);
    clientApp.Start(Seconds(startTime));
    clientApp.Stop(Seconds(DUREE_SIMULATION));

    // Enregistrement de la source
    g_trafficSources.push_back({"Caméra", clientApp.Get(0)});

    Ipv4Address serverIp = GetFirstIpv4Address(serverNode);
    InetSocketAddress sinkSocket(serverIp, 9001);
    InstallSinkIfNeeded(serverNode, sinkSocket, "ns3::UdpSocketFactory");
}


// 2. Capteur de Température (TCP Sporadique, Port 9002)
void ConfigureSensor(Ptr<Node> clientNode, Ptr<Node> serverNode, double startTime)
{
    Ipv4Address remoteIp = GetFirstIpv4Address(serverNode);
    InetSocketAddress remoteSocket(remoteIp, 9002);
    OnOffHelper clientHelper("ns3::TcpSocketFactory", remoteSocket);

    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("50kbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(80));

    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=0.5]")); 
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=300.0]"));

    ApplicationContainer clientApp = clientHelper.Install(clientNode);
    clientApp.Start(Seconds(startTime));
    clientApp.Stop(Seconds(DUREE_SIMULATION));

    // Enregistrement de la source
    g_trafficSources.push_back({"Capteur", clientApp.Get(0)});

    Ipv4Address serverIp = GetFirstIpv4Address(serverNode);
    InetSocketAddress sinkSocket(serverIp, 9002);
    InstallSinkIfNeeded(serverNode, sinkSocket, "ns3::TcpSocketFactory");
}


// 3. Assistant Vocal (TCP Rafale Courte, Port 9003)
void ConfigureVoiceAssistant(Ptr<Node> clientNode, Ptr<Node> serverNode, double startTime)
{
    Ipv4Address remoteIp = GetFirstIpv4Address(serverNode); // Utilise le helper
    InetSocketAddress remoteSocket(remoteIp, 9003);
    OnOffHelper clientHelper("ns3::TcpSocketFactory", remoteSocket);

    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=2.0]")); 
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=60.0]"));
    
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("150kbps"))); 
    clientHelper.SetAttribute("PacketSize", UintegerValue(500)); 

    ApplicationContainer clientApp = clientHelper.Install(clientNode);
    clientApp.Start(Seconds(startTime));
    clientApp.Stop(Seconds(DUREE_SIMULATION));
    
    // Enregistrement de la source
    g_trafficSources.push_back({"AssistantVocal", clientApp.Get(0)});

    Ipv4Address serverIp = GetFirstIpv4Address(serverNode); // Utilise le helper
    InetSocketAddress sinkSocket(serverIp, 9003);
    InstallSinkIfNeeded(serverNode, sinkSocket, "ns3::TcpSocketFactory");
}


// 4. Téléchargement de Fichier (TCP Débit Maximal, Port 9004)
void ConfigureDownload(Ptr<Node> clientNode, Ptr<Node> serverNode, double startTime)
{
    Ipv4Address clientIp = GetFirstIpv4Address(clientNode); // Utilise le helper
    InetSocketAddress remoteSocket(clientIp, 9004);
    BulkSendHelper serverHelper("ns3::TcpSocketFactory", remoteSocket);
    
    serverHelper.SetAttribute("MaxBytes", UintegerValue(100000000));
    
    ApplicationContainer serverApp = serverHelper.Install(serverNode);
    serverApp.Start(Seconds(startTime));
    serverApp.Stop(Seconds(DUREE_SIMULATION));

    // Enregistrement de la source (le serveur dans ce cas)
    g_trafficSources.push_back({"Téléchargement-Serveur", serverApp.Get(0)});

    Ipv4Address sinkIp = GetFirstIpv4Address(clientNode); // Utilise le helper
    InetSocketAddress sinkSocket(sinkIp, 9004);
    InstallSinkIfNeeded(clientNode, sinkSocket, "ns3::TcpSocketFactory");
}


// 5. VoIP (UDP Symétrique, Port 9005 et 9006 pour distinction)
void ConfigureVoIP(Ptr<Node> clientNode, Ptr<Node> serverNode, double startTime)
{
    // Sens 1 : Client -> Serveur (Upload, Port 9005)
    Ipv4Address serverIp = GetFirstIpv4Address(serverNode); // Utilise le helper
    InetSocketAddress upSocket(serverIp, 9005);
    OnOffHelper upHelper("ns3::UdpSocketFactory", upSocket);
    upHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]")); 
    upHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]")); 
    upHelper.SetAttribute("PacketSize", UintegerValue(180)); 
    upHelper.SetAttribute("DataRate", DataRateValue(DataRate("72kbps"))); 

    ApplicationContainer upApp = upHelper.Install(clientNode);
    upApp.Start(Seconds(startTime));
    upApp.Stop(Seconds(DUREE_SIMULATION));
    g_trafficSources.push_back({"VoIP_Montante_Client", upApp.Get(0)});

    // Sens 2 : Serveur -> Client (Download, Port 9006)
    Ipv4Address clientIp = GetFirstIpv4Address(clientNode); // Utilise le helper
    InetSocketAddress downSocket(clientIp, 9006);
    OnOffHelper downHelper("ns3::UdpSocketFactory", downSocket);
    downHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]")); 
    downHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    downHelper.SetAttribute("PacketSize", UintegerValue(180)); 
    downHelper.SetAttribute("DataRate", DataRateValue(DataRate("72kbps"))); 
    
    ApplicationContainer downApp = downHelper.Install(serverNode);
    downApp.Start(Seconds(startTime));
    downApp.Stop(Seconds(DUREE_SIMULATION));
    g_trafficSources.push_back({"VoIP_Descendante_Server", downApp.Get(0)});
    
    // Les Sinks sont installés pour recevoir des deux côtés
    InetSocketAddress sinkSocket1(serverIp, 9005);
    InetSocketAddress sinkSocket2(clientIp, 9006);
    
    InstallSinkIfNeeded(serverNode, sinkSocket1, "ns3::UdpSocketFactory");
    InstallSinkIfNeeded(clientNode, sinkSocket2, "ns3::UdpSocketFactory");
}


// 6. Domotique (UDP Aléatoire, Petit Paquet, Port 9007)
void ConfigureDomotics(Ptr<Node> clientNode, Ptr<Node> serverNode, double startTime)
{
    Ipv4Address remoteIp = GetFirstIpv4Address(serverNode); // Utilise le helper
    InetSocketAddress remoteSocket(remoteIp, 9007);
    UdpClientHelper clientHelper(remoteSocket);
    
    Ptr<UniformRandomVariable> intervalVar = CreateObject<UniformRandomVariable>();
    intervalVar->SetAttribute("Min", DoubleValue(10.0));
    intervalVar->SetAttribute("Max", DoubleValue(30.0));
    double intervalSeconds = intervalVar->GetValue();
    clientHelper.SetAttribute("Interval", TimeValue(Seconds(intervalSeconds)));
    clientHelper.SetAttribute("PacketSize", UintegerValue(64));
    clientHelper.SetAttribute("MaxPackets", UintegerValue(10000)); 

    ApplicationContainer clientApp = clientHelper.Install(clientNode);
    clientApp.Start(Seconds(startTime));
    clientApp.Stop(Seconds(DUREE_SIMULATION));
    
    // Enregistrement de la source
    g_trafficSources.push_back({"Domotique", clientApp.Get(0)});

    Ipv4Address serverIp = GetFirstIpv4Address(serverNode); // Utilise le helper
    InetSocketAddress sinkSocket(serverIp, 9007);
    InstallSinkIfNeeded(serverNode, sinkSocket, "ns3::UdpSocketFactory");
}


// 7. Streaming Musical (TCP Rafale Intermittente, Port 9008)
void ConfigureStreaming(Ptr<Node> clientNode, Ptr<Node> serverNode, double startTime)
{
    Ipv4Address clientIp = GetFirstIpv4Address(clientNode); // Utilise le helper
    InetSocketAddress remoteSocket(clientIp, 9008);
    OnOffHelper serverHelper("ns3::TcpSocketFactory", remoteSocket);

    serverHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=10.0]")); 
    serverHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));

    serverHelper.SetAttribute("DataRate", DataRateValue(DataRate("2Mbps"))); 
    serverHelper.SetAttribute("PacketSize", UintegerValue(1400)); 

    ApplicationContainer serverApp = serverHelper.Install(serverNode);
    serverApp.Start(Seconds(startTime));
    serverApp.Stop(Seconds(DUREE_SIMULATION));

    // Enregistrement de la source
    g_trafficSources.push_back({"Diffusion-Serveur", serverApp.Get(0)});

    Ipv4Address sinkIp = GetFirstIpv4Address(clientNode); // Utilise le helper
    InetSocketAddress sinkSocket(sinkIp, 9008);
    InstallSinkIfNeeded(clientNode, sinkSocket, "ns3::TcpSocketFactory");
}


// 8. Sonnette Connectée (TCP, Très Sporadique, Port 9009)
void ConfigureDoorbell(Ptr<Node> clientNode, Ptr<Node> serverNode, double startTime)
{
    Ipv4Address remoteIp = GetFirstIpv4Address(serverNode); // Utilise le helper
    InetSocketAddress remoteSocket(remoteIp, 9009);
    OnOffHelper clientHelper("ns3::TcpSocketFactory", remoteSocket);

    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("100kbps"))); 
    clientHelper.SetAttribute("PacketSize", UintegerValue(250)); 
    
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]")); 
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=3600.0]")); 

    Ptr<UniformRandomVariable> doorbellStartTime = CreateObject<UniformRandomVariable> ();
    doorbellStartTime->SetAttribute("Min", DoubleValue(0.0));
    doorbellStartTime->SetAttribute("Max", DoubleValue(180.0)); 
    
    ApplicationContainer clientApp = clientHelper.Install(clientNode);
    clientApp.Start(Seconds(doorbellStartTime->GetValue())); 
    clientApp.Stop(Seconds(DUREE_SIMULATION));
    
    // Enregistrement de la source
    g_trafficSources.push_back({"Sonnette", clientApp.Get(0)});

    Ipv4Address serverIp = GetFirstIpv4Address(serverNode); // Utilise le helper
    InetSocketAddress sinkSocket(serverIp, 9009);
    InstallSinkIfNeeded(serverNode, sinkSocket, "ns3::TcpSocketFactory");
}


// 9. Mise à Jour Firmware (TCP, Événement Lourd, Port 9010)
void ConfigureFirmwareUpdate(Ptr<Node> clientNode, Ptr<Node> serverNode, double startTime)
{
    Ipv4Address clientIp = GetFirstIpv4Address(clientNode); // Utilise le helper
    InetSocketAddress remoteSocket(clientIp, 9010);
    BulkSendHelper serverHelper("ns3::TcpSocketFactory", remoteSocket);
    
    serverHelper.SetAttribute("MaxBytes", UintegerValue(500000000));
    
    double updateStartTime = 300.0; 
    
    ApplicationContainer serverApp = serverHelper.Install(serverNode);
    serverApp.Start(Seconds(updateStartTime)); 
    serverApp.Stop(Seconds(DUREE_SIMULATION));

    // Enregistrement de la source
    g_trafficSources.push_back({"MiseAJourFirmware-Serveur", serverApp.Get(0)});

    Ipv4Address sinkIp = GetFirstIpv4Address(clientNode); // Utilise le helper
    InetSocketAddress sinkSocket(sinkIp, 9010);
    InstallSinkIfNeeded(clientNode, sinkSocket, "ns3::TcpSocketFactory");
}


// 10. Monitoring Réseau (UDP, Régulier Léger, Port 9011)
void ConfigureMonitoring(Ptr<Node> clientNode, Ptr<Node> serverNode, double startTime)
{
    Ipv4Address remoteIp = GetFirstIpv4Address(serverNode); // Utilise le helper
    InetSocketAddress remoteSocket(remoteIp, 9011);
    UdpClientHelper clientHelper(remoteSocket);

    clientHelper.SetAttribute("Interval", TimeValue(Seconds(2.0))); 
    clientHelper.SetAttribute("PacketSize", UintegerValue(300)); 
    clientHelper.SetAttribute("MaxPackets", UintegerValue(10000));

    ApplicationContainer clientApp = clientHelper.Install(clientNode);
    clientApp.Start(Seconds(startTime));
    clientApp.Stop(Seconds(DUREE_SIMULATION));

    // Enregistrement de la source
    g_trafficSources.push_back({"Supervision", clientApp.Get(0)});

    Ipv4Address serverIp = GetFirstIpv4Address(serverNode); // Utilise le helper
    InetSocketAddress sinkSocket(serverIp, 9011);
    InstallSinkIfNeeded(serverNode, sinkSocket, "ns3::UdpSocketFactory");
}

/**
 * @brief Calcule et affiche les métriques de performance pour chaque application.
 * * Cette fonction itère sur tous les sinks installés (récepteurs) et calcule :
 * 1. Le débit : Octets reçus * 8 / (Durée de simulation * 10^6) -> Mbits/s
 * 2. Le taux de perte de paquets (Loss Rate) : Non calculé actuellement (N/A)
 */
void CalculateMetrics(Ptr<FlowMonitor> monitor = 0, Ptr<Ipv4FlowClassifier> classifier = 0, bool enableCsv = false, const std::string &csvOutput = "simulation-domestique-metrics.csv")
{
    // Ouvrir un fichier pour sauvegarder les résultats (XML)
    std::ofstream resultsFile;
    // Changement du format de sortie vers XML
    resultsFile.open("simulation-domestique-metrics.xml");
    
    // Écrire le préambule XML et la balise racine
    resultsFile << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
    resultsFile << "<SimulationMetrics duration_seconds=\"" << DUREE_SIMULATION << "\">" << std::endl;
    
    

    // Étape 2 : Collecter les métriques de réception pour tous les sinks
    for (const auto& pairNode : g_installedSinks)
    {
        uint32_t nodeId = pairNode.first;
        for (const auto& pairPort : pairNode.second)
        {
            uint16_t port = pairPort.first;
            Ptr<PacketSink> sink = pairPort.second;
            
            if (!sink) continue;

            // Données de réception du sink
            uint64_t totalReceivedBytes = sink->GetTotalRx();
            
            // Débit (Octets/s -> Mbits/s)
            // Débit = (Octets reçus * 8) / (Durée de simulation * 10^6)
            double throughputMbps = (totalReceivedBytes * 8.0) / (DUREE_SIMULATION * 1000000.0);
            
            // Trouver le type d'application basé sur le port (approximatif)
            std::string appType = "Inconnu";
            switch (port)
            {
                case 9001: appType = "Caméra"; break;
                case 9002: appType = "Capteur"; break;
                case 9003: appType = "AssistantVocal"; break;
                case 9004: appType = "Téléchargement"; break;
                case 9005: appType = "VoIP_LiaisonMontante"; break;
                case 9006: appType = "VoIP_LiaisonDescendante"; break;
                case 9007: appType = "Domotique"; break;
                case 9008: appType = "Diffusion"; break;
                case 9009: appType = "Sonnette"; break;
                case 9010: appType = "MiseAJourFirmware"; break;
                case 9011: appType = "Supervision"; break;
            }
            
            // Recueillir métriques additionnelles si FlowMonitor/Classifier disponibles
            double lossPct = 0.0;
            double meanDelayMs = 0.0;
            double meanJitterMs = 0.0;
            uint64_t txPacketsAgg = 0, rxPacketsAgg = 0, lostPacketsAgg = 0;
            uint64_t txBytesAgg = 0, rxBytesAgg = 0;
            if (monitor && classifier)
            {
                // Aggréger les statistiques de flow correspondant à ce sink (destination = node IP & port)
                Ptr<Node> n = NodeList::GetNode(nodeId);
                Ipv4Address nodeIp = GetFirstIpv4Address(n);
                std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
                for (auto &kv2 : stats)
                {
                    FlowId fId = kv2.first;
                    FlowMonitor::FlowStats fs = kv2.second;
                    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(fId);
                    if ((t.destinationAddress == nodeIp && t.destinationPort == port) || (t.sourceAddress == nodeIp && t.sourcePort == port))
                    {
                        txPacketsAgg += fs.txPackets;
                        rxPacketsAgg += fs.rxPackets;
                        lostPacketsAgg += fs.lostPackets;
                        txBytesAgg += fs.txBytes;
                        rxBytesAgg += fs.rxBytes;
                    }
                }
                // Calculer perte et moyenne de délai/jitter si possible
                if (txPacketsAgg > 0)
                {
                    lossPct = (double)(txPacketsAgg - rxPacketsAgg) * 100.0 / (double)txPacketsAgg;
                }
                if (rxPacketsAgg > 0)
                {
                    // Pour delay/jitter, nous allons calculer via toutes les flows correspondantes
                    Time delaySum = Seconds(0.0);
                    Time jitterSum = Seconds(0.0);
                    for (auto &kv2 : stats)
                    {
                        FlowMonitor::FlowStats fs = kv2.second;
                        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(kv2.first);
                        if ((t.destinationAddress == nodeIp && t.destinationPort == port) || (t.sourceAddress == nodeIp && t.sourcePort == port))
                        {
                            delaySum += fs.delaySum;
                            jitterSum += fs.jitterSum;
                        }
                    }
                    meanDelayMs = (delaySum.GetSeconds() / (double)rxPacketsAgg) * 1000.0;
                    meanJitterMs = (jitterSum.GetSeconds() / (double)rxPacketsAgg) * 1000.0;
                }
            }

            // Affichage dans la console
            std::cout << appType << " (Nœud " << nodeId << ", Port " << port << ") | Reçu: " 
                      << totalReceivedBytes << " Octets | Débit: " 
                      << std::fixed << std::setprecision(3) << throughputMbps << " Mbps";
            if (monitor && classifier) {
                std::cout << " | Perte: " << std::fixed << std::setprecision(3) << lossPct << "% | Délai moyen: " << meanDelayMs << " ms | Jitter moyen: " << meanJitterMs << " ms";
            }
            std::cout << std::endl;

            // Sauvegarde dans le fichier au format XML
            resultsFile << "  <Result type=\"" << appType 
                        << "\" nodeId=\"" << nodeId 
                        << "\" port=\"" << port 
                        << "\" octetsRecus=\"" << totalReceivedBytes 
                        << "\" debitMbps=\"" << std::fixed << std::setprecision(3) << throughputMbps 
                        << "\" tauxPertePct=\"" << std::fixed << std::setprecision(3) << lossPct 
                        << "\" moyenneDelaiMs=\"" << std::fixed << std::setprecision(3) << meanDelayMs 
                        << "\" moyenneJitterMs=\"" << std::fixed << std::setprecision(3) << meanJitterMs 
                        << "\" />" << std::endl;
        }
    }
    
    resultsFile << "</SimulationMetrics>" << std::endl;
    resultsFile.close();
    // Si demandé, ouvrir et écrire un CSV avec des métriques détaillées de flux via FlowMonitor
    if (enableCsv && monitor && classifier)
    {
        // Préparer le classifier et récupérer les statistiques
        // 'classifier' doit provenir de FlowMonitorHelper créé par RunSimulation
        std::ofstream csvFile;
        csvFile.open(csvOutput);
        csvFile << "flowId,srcAddr,srcPort,dstAddr,dstPort,txPackets,rxPackets,lostPackets,lossPct,txBytes,rxBytes,throughputMbps,meanDelayMs,meanJitterMs" << std::endl;
        std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
        for (auto &kv : stats)
        {
            FlowId flowId = kv.first;
            FlowMonitor::FlowStats fs = kv.second;
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowId);
            double lossPct = 0.0;
            if (fs.txPackets > 0)
            {
                lossPct = (double)(fs.txPackets - fs.rxPackets) * 100.0 / (double)fs.txPackets;
            }
            double durationSeconds = (fs.timeLastTxPacket.GetSeconds() - fs.timeFirstTxPacket.GetSeconds());
            double throughputMbps = 0.0;
            if (durationSeconds > 0.0)
            {
                throughputMbps = (fs.rxBytes * 8.0) / (durationSeconds * 1000000.0);
            }
            double meanDelayMs = 0.0;
            if (fs.rxPackets > 0)
            {
                meanDelayMs = (fs.delaySum.GetSeconds() / (double)fs.rxPackets) * 1000.0;
            }
            double meanJitterMs = 0.0;
            if (fs.rxPackets > 0)
            {
                meanJitterMs = (fs.jitterSum.GetSeconds() / (double)fs.rxPackets) * 1000.0;
            }
            csvFile << flowId << "," << t.sourceAddress << "," << t.sourcePort << "," << t.destinationAddress << "," << t.destinationPort << ","
                    << fs.txPackets << "," << fs.rxPackets << "," << fs.lostPackets << "," << lossPct << "," << fs.txBytes << "," << fs.rxBytes << "," << std::fixed << std::setprecision(6) << throughputMbps << "," << meanDelayMs << "," << meanJitterMs << std::endl;
        }
        csvFile.close();
        NS_LOG_INFO("CSV des métriques FlowMonitor sauvegardées dans " << csvOutput);
        // Ajouter un résumé par application (sink) si demandé
        std::ofstream csvSummary;
        std::string summaryName = std::string("summary-") + csvOutput;
        csvSummary.open(summaryName);
        csvSummary << "nodeId,port,appType,txPackets,rxPackets,lostPackets,lossPct,txBytes,rxBytes,throughputMbps,meanDelayMs,meanJitterMs" << std::endl;
        std::map<FlowId, FlowMonitor::FlowStats> stats2 = monitor->GetFlowStats();
        for (const auto& pairNode : g_installedSinks)
        {
            uint32_t nodeId = pairNode.first;
            Ptr<Node> n = NodeList::GetNode(nodeId);
            Ipv4Address nodeIp = GetFirstIpv4Address(n);
            for (const auto& pairPort : pairNode.second)
            {
                uint16_t port = pairPort.first;
                uint64_t txPacketsAgg = 0, rxPacketsAgg = 0, lostPacketsAgg = 0;
                uint64_t txBytesAgg = 0, rxBytesAgg = 0;
                Time delaySum = Seconds(0.0);
                Time jitterSum = Seconds(0.0);
                for (auto &kv2 : stats2)
                {
                    FlowMonitor::FlowStats fs = kv2.second;
                    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(kv2.first);
                    if ((t.destinationAddress == nodeIp && t.destinationPort == port) || (t.sourceAddress == nodeIp && t.sourcePort == port))
                    {
                        txPacketsAgg += fs.txPackets;
                        rxPacketsAgg += fs.rxPackets;
                        lostPacketsAgg += fs.lostPackets;
                        txBytesAgg += fs.txBytes;
                        rxBytesAgg += fs.rxBytes;
                        delaySum += fs.delaySum;
                        jitterSum += fs.jitterSum;
                    }
                }
                double lossPct = 0.0;
                if (txPacketsAgg > 0) lossPct = (double)(txPacketsAgg - rxPacketsAgg) * 100.0 / (double)txPacketsAgg;
                double meanDelayMs = 0.0, meanJitterMs = 0.0, throughputMbps = 0.0;
                if (rxPacketsAgg > 0) {
                    meanDelayMs = (delaySum.GetSeconds() / (double)rxPacketsAgg) * 1000.0;
                    meanJitterMs = (jitterSum.GetSeconds() / (double)rxPacketsAgg) * 1000.0;
                }
                if (DUREE_SIMULATION > 0)
                {
                    throughputMbps = (rxBytesAgg * 8.0) / (DUREE_SIMULATION * 1000000.0);
                }
                // Déduire le type d'application (même mapping que plus haut)
                std::string appType = "Inconnu";
                switch (port)
                {
                    case 9001: appType = "Caméra"; break;
                    case 9002: appType = "Capteur"; break;
                    case 9003: appType = "AssistantVocal"; break;
                    case 9004: appType = "Téléchargement"; break;
                    case 9005: appType = "VoIP_LiaisonMontante"; break;
                    case 9006: appType = "VoIP_LiaisonDescendante"; break;
                    case 9007: appType = "Domotique"; break;
                    case 9008: appType = "Diffusion"; break;
                    case 9009: appType = "Sonnette"; break;
                    case 9010: appType = "MiseAJourFirmware"; break;
                    case 9011: appType = "Supervision"; break;
                }
                csvSummary << nodeId << "," << port << "," << appType << "," << txPacketsAgg << "," << rxPacketsAgg << "," << lostPacketsAgg << "," << lossPct << "," << txBytesAgg << "," << rxBytesAgg << "," << throughputMbps << "," << meanDelayMs << "," << meanJitterMs << std::endl;
            }
        }
        csvSummary.close();
        NS_LOG_INFO("Résumé CSV par application sauvegardé dans " << summaryName);
    }
    NS_LOG_INFO("Métriques sauvegardées dans simulation-domestique-metrics.xml");
}


void RunSimulation(bool forceAc, bool enableFlowMonitor, const std::string &flowOutput, bool enablePcap, bool enableCsv, const std::string &csvOutput)
{
    // --- 1. Création des Nœuds ---
    NodeContainer clientNodes;
    clientNodes.Create (N_EQUIPMENTS); 

    NodeContainer serverNodes;
    serverNodes.Create (K_TYPES); 

    Ptr<Node> apNode = CreateObject<Node> (); 

    // --- 2. Configuration du Canal Wi-Fi & Mobilité ---
    YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
    Ptr<YansWifiChannel> channel = channelHelper.Create();
    
    // Piste d'amélioration : modèle de propagation plus réaliste pour le domaine
    // channelHelper.AddPropagationLoss("ns3::LogDistancePropagationLossModel", 
    //                                  "Exponent", DoubleValue(3.0)); // Ajout de murs/distance
    
    YansWifiPhyHelper phyHelper;
    phyHelper.SetChannel(channel);
   
    // Piste d'amélioration : configuration de la sensibilité du récepteur (si vous étudiez la portée)
    // phyHelper.Set("EnergyDetectionThreshold", DoubleValue(-96.0));
    
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(clientNodes);
    mobility.Install(serverNodes); 
    
    // --- 3. Configuration Wi-Fi ---
    WifiHelper wifiHelper;
    if (forceAc)
    {
        wifiHelper.SetStandard(WIFI_STANDARD_80211ac);
        NS_LOG_INFO("J'impose le standard Wi‑Fi : 802.11ac");
    }
    else
    {
        NS_LOG_INFO("Utilisation du standard Wi‑Fi par défaut (sans contrainte). ");
    }

    
    // Utilisation de Minstrel HT manager pour les modes haut débit (supporte HT/VHT)
    wifiHelper.SetRemoteStationManager("ns3::MinstrelHtWifiManager");

    
    WifiMacHelper macHelper;
    Ssid ssid = Ssid("MaisonConnectee");

    // a) Configuration AP
    macHelper.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifiHelper.Install(phyHelper, macHelper, apNode);

    // b) Configuration Clients
    macHelper.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer clientDevices = wifiHelper.Install(phyHelper, macHelper, clientNodes);

    // Diagnostic : affichage de la configuration (le reste est inchangé)
    Ptr<WifiNetDevice> apWifiDev = DynamicCast<WifiNetDevice>(apDevice.Get(0));
    if (apWifiDev)
    {
            NS_LOG_INFO("Type de WifiNetDevice AP : " << apWifiDev->GetInstanceTypeId().GetName());
        Ptr<WifiPhy> apPhy = apWifiDev->GetPhy();
        if (apPhy)
        {
            NS_LOG_INFO("Type Phy AP : " << apPhy->GetInstanceTypeId().GetName());
            NS_LOG_INFO("Standard Phy AP configuré : " << apPhy->GetStandard());
            NS_LOG_INFO("Bande Phy AP : " << apPhy->GetPhyBand());
        }
        Ptr<WifiRemoteStationManager> apRsm = apWifiDev->GetRemoteStationManager();
        if (apRsm)
        {
            NS_LOG_INFO("RemoteStationManager AP : " << apRsm->GetInstanceTypeId().GetName());
        }
    }
    Ptr<WifiNetDevice> staWifiDev = DynamicCast<WifiNetDevice>(clientDevices.Get(0));
    if (staWifiDev)
    {
        NS_LOG_INFO("Type de WifiNetDevice STA : " << staWifiDev->GetInstanceTypeId().GetName());
        Ptr<WifiPhy> staPhy = staWifiDev->GetPhy();
        if (staPhy)
        {
            NS_LOG_INFO("Type Phy STA : " << staPhy->GetInstanceTypeId().GetName());
            NS_LOG_INFO("Standard Phy STA configuré : " << staPhy->GetStandard());
            NS_LOG_INFO("Bande Phy STA : " << staPhy->GetPhyBand());
        }
        Ptr<WifiRemoteStationManager> staRsm = staWifiDev->GetRemoteStationManager();
        if (staRsm)
        {
            NS_LOG_INFO("RemoteStationManager STA : " << staRsm->GetInstanceTypeId().GetName());
        }
    }
    
    // --- 4. Configuration Réseau Serveurs (Ethernet) ---
    PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("DataRate", StringValue("1000Mbps"));
    p2pHelper.SetChannelAttribute("Delay", StringValue("1ms"));
    
    NetDeviceContainer apP2pDevices;
    NetDeviceContainer serverDevices;
    std::vector<NetDeviceContainer> p2pLinks;

    // Utiliser un réseau /24 différent par lien point-à-point (10.2.x.0/24 ...)
    Ipv4AddressHelper p2pAddress;
    p2pAddress.SetBase("10.2.1.0", "255.255.255.0");
    
    for (uint32_t i = 0; i < K_TYPES; ++i)
    {
        NetDeviceContainer link = p2pHelper.Install(apNode, serverNodes.Get(i));
        apP2pDevices.Add(link.Get(0));
        serverDevices.Add(link.Get(1));
        p2pLinks.push_back(link);
    }

    // --- 5. Installation de la Pile Internet (IP) ---
    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(clientNodes);
    stack.Install(serverNodes);

    // Adressage stratégique : 10.1.1.0/24
    Ipv4AddressHelper address; 
    address.SetBase ("10.1.1.0", "255.255.255.0"); // Fixed subnet mask here
    
    // Attribution des adresses IP aux interfaces
    address.Assign(apDevice);
    address.Assign(clientDevices);

    // Assigner des adresses IP à chaque lien point-à-point créé plus tôt
    for (auto &link : p2pLinks)
    {
        p2pAddress.Assign(link);
        p2pAddress.NewNetwork();
    }
    // serverDevices were assigned via the p2pAddress per-link above

    // Remplir les tables de routage globales pour permettre le routage entre AP et liens point-à-point
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    
    // Débogage : afficher les adresses IP (inchangé)
    NS_LOG_INFO ("Adresses assignées pour les serveurs et clients :");
    for (uint32_t i = 0; i < serverNodes.GetN (); ++i)
    {
        Ptr<Node> node = serverNodes.Get (i);
        Ipv4Address addr = GetFirstIpv4Address(node);
        NS_LOG_INFO ("Serveur " << i << " -> " << addr);
    }
    for (uint32_t i = 0; i < clientNodes.GetN (); ++i)
    {
        Ptr<Node> node = clientNodes.Get (i);
        Ipv4Address addr = GetFirstIpv4Address(node);
        NS_LOG_INFO ("Client " << i << " -> " << addr);
    }
    

    // --- 6. Déploiement des Applications ---
    uint32_t nextClientIndex = 0; 
    
    // Configuration de chaque type d'application (clients N_EQUIPMENTS = 32)
    for (uint32_t i = 0; i < 5; ++i) { ConfigureCamera(clientNodes.Get(nextClientIndex++), serverNodes.Get(0), debutAleatoire->GetValue()); } // 5 caméras
    for (uint32_t i = 0; i < 10; ++i) { ConfigureSensor(clientNodes.Get(nextClientIndex++), serverNodes.Get(1), debutAleatoire->GetValue()); } // 10 capteurs
    for (uint32_t i = 0; i < 3; ++i) { ConfigureVoiceAssistant(clientNodes.Get(nextClientIndex++), serverNodes.Get(2), debutAleatoire->GetValue()); } // 3 assistants vocaux
    for (uint32_t i = 0; i < 2; ++i) { ConfigureDownload(clientNodes.Get(nextClientIndex++), serverNodes.Get(3), debutAleatoire->GetValue()); } // 2 téléchargements
    for (uint32_t i = 0; i < 4; ++i) { ConfigureVoIP(clientNodes.Get(nextClientIndex++), serverNodes.Get(4), debutAleatoire->GetValue()); } // 4 sessions VoIP
    for (uint32_t i = 0; i < 4; ++i) { ConfigureDomotics(clientNodes.Get(nextClientIndex++), serverNodes.Get(5), debutAleatoire->GetValue()); } // 4 domotiques
    ConfigureStreaming(clientNodes.Get(nextClientIndex++), serverNodes.Get(6), debutAleatoire->GetValue()); // 1 streaming
    ConfigureDoorbell(clientNodes.Get(nextClientIndex++), serverNodes.Get(7), debutAleatoire->GetValue()); // 1 sonnette
    ConfigureFirmwareUpdate(clientNodes.Get(nextClientIndex++), serverNodes.Get(8), debutAleatoire->GetValue()); // 1 màj
    ConfigureMonitoring(clientNodes.Get(nextClientIndex++), serverNodes.Get(9), debutAleatoire->GetValue()); // 1 monitoring

    NS_ASSERT (nextClientIndex == N_EQUIPMENTS); 

    // --- 7. FlowMonitor (optionnel) ---
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor;
    if (enableFlowMonitor)
    {
        NS_LOG_INFO("Installation du FlowMonitor sur tous les nœuds");
        monitor = flowmon.InstallAll();
        // Récupérer le classifieur qui a été utilisé pour identifier les flux
        Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    }

    // --- 8. Collecte de Traces PCAP ---
    if (enablePcap)
    {
        NS_LOG_INFO("Activation de la capture PCAP : traces-simulation-domestique*");
        phyHelper.EnablePcapAll("traces-simulation-domestique", true);
    }

    // --- 8. Lancement de la Simulation ---
    Simulator::Stop (Seconds(DUREE_SIMULATION));
    Simulator::Run ();
    
    // --- Optionnel : sérialisation du FlowMonitor ---
    if (enableFlowMonitor)
    {
        if (monitor)
        {
            monitor->CheckForLostPackets();
            flowmon.SerializeToXmlFile(flowOutput, true, true);
            NS_LOG_INFO("FlowMonitor enregistré dans " << flowOutput);
        }
        else
        {
            NS_LOG_WARN("FlowMonitor activé mais le pointeur 'monitor' est nul");
        }
    }

    // --- 9. Post-traitement et Extraction de Métriques ---
        // Appeler le calcul des métriques en transmettant le monitor et le classifier si disponibles
        Ptr<Ipv4FlowClassifier> classifierPtr = 0;
        if (enableFlowMonitor) {
            classifierPtr = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
        }
        CalculateMetrics(monitor, classifierPtr, enableCsv, csvOutput);
}


// --- Le point d'entrée du programme C++ ---
int main (int argc, char *argv[])
{
    // LogLevel INFO pour la phase de configuration
    LogComponentEnable("SimulationDomestique", LOG_LEVEL_INFO);

    // Initialisation du générateur aléatoire
    debutAleatoire->SetAttribute("Min", DoubleValue(0.0));
    debutAleatoire->SetAttribute("Max", DoubleValue(5.0)); // Aléa [0, 5] secondes

    // Désactive les avertissements de la table de routage (recommandé pour ce type de topo)
    LogComponentEnable("Ipv4GlobalRouting", LOG_LEVEL_WARN);

    // Paramètres CLI
    bool forceAc = true;
    bool enableFlowMonitor = false;
    std::string flowOutput = "traces_de_simulation.xml";
    // Option to control the simulation duration, PCAP capture et CSV
    bool enablePcap = false; // disabled by default to avoid large files
    double duration = DUREE_SIMULATION;
    bool enableCsv = false;
    std::string csvOutput = "simulation-domestique-metrics.csv";

    CommandLine cmd;
    cmd.AddValue("forceAc", "Force Wi-Fi standard to 802.11ac", forceAc);
    cmd.AddValue("enableFlowMonitor", "Enable FlowMonitor and write XML", enableFlowMonitor);
    cmd.AddValue("flowOutput", "FlowMonitor output filename", flowOutput);
    cmd.AddValue("enableCsv", "Enable CSV export of FlowMonitor metrics", enableCsv);
    cmd.AddValue("csvOutput", "CSV output filename if enableCsv=true", csvOutput);
    cmd.AddValue("duration", "Simulation duration in seconds", duration);
    cmd.AddValue("enablePcap", "Enable PCAP capture (can generate large files)", enablePcap);
    cmd.Parse(argc, argv);

    // Appliquer les options spécifiées en CLI
    DUREE_SIMULATION = duration;
    RunSimulation(forceAc, enableFlowMonitor, flowOutput, enablePcap, enableCsv, csvOutput);

    Simulator::Destroy ();
    return 0;
}
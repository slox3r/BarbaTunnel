#include "StdAfx.h"
#include "BarbaClientUdpConnection.h"
#include "BarbaClientApp.h"


BarbaClientUdpConnection::BarbaClientUdpConnection(BarbaClientConfig* config, BarbaClientConfigItem* configItem, u_short clientPort, u_short tunnelPort)
	: BarbaClientConnection(config, configItem)
{
	this->ClientPort = clientPort;
	this->TunnelPort = tunnelPort;
}

u_short BarbaClientUdpConnection::GetTunnelPort()
{
	return this->TunnelPort;
}

BarbaClientUdpConnection::~BarbaClientUdpConnection(void)
{
}

bool BarbaClientUdpConnection::ShouldProcessPacket(PacketHelper* packet)
{
	//check outgoing packets
	if (packet->GetDesIp()==GetServerIp())
	{
		return ConfigItem->ShouldGrabPacket(packet);
	}
	//check incoming packets
	else if (packet->GetSrcIp()==GetServerIp())
	{
		return packet->ipHeader->ip_p==this->ConfigItem->GetTunnelProtocol()
			&& packet->GetSrcPort()==this->TunnelPort;
	}
	else
	{
		return false;
	}
}

bool BarbaClientUdpConnection::ExtractUdpBarbaPacket(PacketHelper* barbaPacket, PacketHelper* orgPacket)
{
	DecryptPacket(barbaPacket);
	orgPacket->SetEthHeader(barbaPacket->ethHeader);
	return orgPacket->SetIpPacket((iphdr_ptr)barbaPacket->GetUdpPayload()) && orgPacket->IsValidChecksum();
}

bool BarbaClientUdpConnection::CreateUdpBarbaPacket(PacketHelper* packet, PacketHelper*  barbaPacket)
{
	packet->RecalculateChecksum();
	barbaPacket->Reset(IPPROTO_UDP);
	barbaPacket->SetEthHeader(packet->ethHeader);
	barbaPacket->ipHeader->ip_ttl = packet->ipHeader->ip_ttl;
	barbaPacket->ipHeader->ip_v = packet->ipHeader->ip_v;
	barbaPacket->ipHeader->ip_id = 56;
	barbaPacket->ipHeader->ip_off = packet->ipHeader->ip_off;
	barbaPacket->SetSrcIp(packet->GetSrcIp());
	barbaPacket->SetDesIp(this->GetServerIp());
	barbaPacket->SetSrcPort(this->ClientPort);
	barbaPacket->SetDesPort(this->TunnelPort);
	barbaPacket->SetUdpPayload((BYTE*)packet->ipHeader, packet->GetIpLen());
	EncryptPacket(barbaPacket);
	return true;
}

bool BarbaClientUdpConnection::ProcessPacket(PacketHelper* packet, bool send)
{
	if (send)
	{
		if (!theApp->CheckMTUDecrement(packet->GetPacketLen(), sizeof iphdr + sizeof tcphdr + sizeof BarbaHeader))
			return false;

		//Create Barba packet
		PacketHelper barbaPacket;
		if (!CreateUdpBarbaPacket(packet, &barbaPacket))
			return false;
		this->SendPacketToAdapter(&barbaPacket);
		return true;
	}
	else
	{
		//extract Barba packet
		PacketHelper orgPacket;
		ExtractUdpBarbaPacket(packet, &orgPacket);
		this->SendPacketToMstcp(&orgPacket);
		return true;
	}
}
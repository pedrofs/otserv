//////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
//////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//////////////////////////////////////////////////////////////////////
#include "otpch.h"

#include "definitions.h"
#include "ban.h"
#include "ioplayer.h"
#include "configmanager.h"
#include <sstream>
#include "tools.h"

#if defined USE_SQL_ENGINE
#include "database.h"
#endif

extern ConfigManager g_config;

IOBan* IOBan::_instance = NULL;

Ban::Ban()
{
	OTSYS_THREAD_LOCKVARINIT(banLock);
}

void Ban::init()
{
	maxLoginTries = (uint32_t)g_config.getNumber(ConfigManager::LOGIN_TRIES);
	retryTimeout = (uint32_t)g_config.getNumber(ConfigManager::RETRY_TIMEOUT) / 1000;
	loginTimeout = (uint32_t)g_config.getNumber(ConfigManager::LOGIN_TIMEOUT) / 1000;
}

bool Ban::isIpBanished(SOCKET s)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(banLock);
	uint32_t clientip = getIPSocket(s);
	if(clientip != 0){
		for(IpBanList::iterator it = ipBanList.begin(); it !=  ipBanList.end(); ++it){
			if((it->ip & it->mask) == (clientip & it->mask)){
				uint32_t currentTime = std::time(NULL);
				if(it->time == 0 || currentTime < it->time){
					return true;
				}
			}
		}
	}

	return false;
}

bool Ban::isIpDisabled(SOCKET s)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(banLock);
	if(maxLoginTries == 0){
		return false;
	}

	uint32_t clientip = getIPSocket(s);
	if(clientip != 0){
		uint32_t currentTime = std::time(NULL);
		IpLoginMap::const_iterator it = ipLoginMap.find(clientip);
		if(it != ipLoginMap.end()){
			if( (it->second.numberOfLogins >= maxLoginTries) &&
				(currentTime < it->second.lastLoginTime + loginTimeout) ){
				return true;
			}
		}
	}

	return false;
}

bool Ban::acceptConnection(SOCKET s)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(banLock);
	uint32_t clientip = getIPSocket(s);

	if(clientip == 0){
		return false;
	}

	uint64_t currentTime = OTSYS_TIME();

	IpConnectMap::iterator it = ipConnectMap.find(clientip);
	if(it == ipConnectMap.end()){
		ConnectBlock cb;
		cb.lastConnection = currentTime;

		ipConnectMap[clientip] = cb;
		return true;
	}

	if(currentTime - it->second.lastConnection < 1000){
		return false;
	}

	it->second.lastConnection = currentTime;
	return true;
}

void Ban::addLoginAttempt(SOCKET s, bool isSuccess)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(banLock);
	uint32_t clientip = getIPSocket(s);
	if(clientip != 0){
		uint32_t currentTime = std::time(NULL);

		IpLoginMap::iterator it = ipLoginMap.find(clientip);
		if(it == ipLoginMap.end()){
			LoginBlock lb;
			lb.lastLoginTime = 0;
			lb.numberOfLogins = 0;

			ipLoginMap[clientip] = lb;
			it = ipLoginMap.find(clientip);
		}

		if(it->second.numberOfLogins >= maxLoginTries){
			it->second.numberOfLogins = 0;
		}

		if(!isSuccess || (currentTime < it->second.lastLoginTime + retryTimeout) ){
			++it->second.numberOfLogins;
		}
		else
			it->second.numberOfLogins = 0;

		it->second.lastLoginTime = currentTime;
	}
}

bool Ban::isPlayerBanished(const std::string& name)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(banLock);
	uint32_t playerId;
	std::string playerName = name;
	if(!IOPlayer::instance()->getGuidByName(playerId, playerName)){
		return false;
	}
	for(PlayerBanList::iterator it = playerBanList.begin(); it !=  playerBanList.end(); ++it){
   		if(it->id  == playerId){
			uint32_t currentTime = std::time(NULL);
			if(it->time == 0 || currentTime < it->time){
				return true;
			}
		}
	}
	return false;
}

bool Ban::isAccountBanished(uint32_t account)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(banLock);
	uint32_t currentTime = std::time(NULL);
	for(AccountBanList::iterator it = accountBanList.begin(); it !=  accountBanList.end(); ++it){
   		if(it->id  == account){
			if(it->time == 0 || currentTime < it->time){
				return true;
			}
		}
	}
	return false;
}

void Ban::addIpBan(uint32_t ip, uint32_t mask, uint32_t time)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(banLock);
	for(IpBanList::iterator it = ipBanList.begin(); it !=  ipBanList.end(); ++it){
		if(it->ip == ip && it->mask == mask){
			it->time = time;
			return;
		}
	}
	IpBanStruct ipBanStruct(ip, mask, time);
	ipBanList.push_back(ipBanStruct);
}

void Ban::addPlayerBan(uint32_t playerId, uint32_t time)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(banLock);
	for(PlayerBanList::iterator it = playerBanList.begin(); it !=  playerBanList.end(); ++it){
		if(it->id == playerId){
			it->time = time;
			return;
		}
	}
	PlayerBanStruct playerBanStruct(playerId, time);
	playerBanList.push_back(playerBanStruct);
}

void Ban::addAccountBan(uint32_t account, uint32_t time)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(banLock);
	for(AccountBanList::iterator it = accountBanList.begin(); it !=  accountBanList.end(); ++it){
		if(it->id == account){
			it->time = time;
			return;
		}
	}
	AccountBanStruct accountBanStruct(account, time);
	accountBanList.push_back(accountBanStruct);
}

bool Ban::removeIpBan(uint32_t n)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(banLock);
	for(IpBanList::iterator it = ipBanList.begin(); it !=  ipBanList.end(); ++it){
		--n;
		if(n == 0){
			ipBanList.erase(it);
			return true;
		}
	}
	return false;
}

bool Ban::removePlayerBan(uint32_t n)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(banLock);
	for(PlayerBanList::iterator it = playerBanList.begin(); it !=  playerBanList.end(); ++it){
		--n;
		if(n == 0){
			playerBanList.erase(it);
			return true;
		}
	}
	return false;
}

bool Ban::removeAccountBan(uint32_t n)
{
	OTSYS_THREAD_LOCK_CLASS lockClass(banLock);
	for(AccountBanList::iterator it = accountBanList.begin(); it !=  accountBanList.end(); ++it){
		--n;
		if(n == 0){
			accountBanList.erase(it);
			return true;
		}
	}
	return false;
}

const IpBanList& Ban::getIpBans()
{
	return ipBanList;
}

const PlayerBanList& Ban::getPlayerBans()
{
	return playerBanList;
}

const AccountBanList& Ban::getAccountBans()
{
	return accountBanList;
}


bool Ban::loadBans(const std::string& identifier)
{
	return IOBan::getInstance()->loadBans(identifier, *this);
}
bool Ban::saveBans(const std::string& identifier)
{
	return IOBan::getInstance()->saveBans(identifier, *this);
}

IOBan* IOBan::getInstance()
{
	if(!_instance){
		#if defined USE_SQL_ENGINE
		_instance = new IOBanSQL();
		#else
		_instance = new IOBanXML();
		#endif
	}
	return _instance;
}

#if defined USE_SQL_ENGINE

IOBanSQL::IOBanSQL()
{
	m_host = g_config.getString(ConfigManager::SQL_HOST);
	m_user = g_config.getString(ConfigManager::SQL_USER);
	m_pass = g_config.getString(ConfigManager::SQL_PASS);
	m_db   = g_config.getString(ConfigManager::SQL_DB);
}


bool IOBanSQL::loadBans(const std::string& identifier, Ban& banclass)
{
	Database* db = Database::instance();
	DBQuery query;
	DBResult result;
	
	if(!db->connect()){
		return false;
	}

	query << "SELECT * FROM bans";
	if(!db->storeQuery(query, result))
		return true;

	uint32_t currentTime = std::time(NULL);
	for(uint32_t i=0; i < result.getNumRows(); ++i){
		int banType = result.getDataInt("type", i);
		int time = result.getDataInt("time", i);
		if(time > (int)currentTime){
			switch(banType){
				case BAN_IPADDRESS:
				{
					int ip = result.getDataInt("ip", i);
					int mask = result.getDataInt("mask", i);
					banclass.addIpBan(ip, mask, time);
					break;
				}

				case BAN_PLAYER:
				{
					int player = result.getDataInt("player", i);
					banclass.addPlayerBan(player, time);
					break;
				}

				case BAN_ACCOUNT:
				{
					int account = result.getDataInt("account", i);
					banclass.addAccountBan(account, time);
					break;
				}
			}
		}
	}

	return true;
}

bool IOBanSQL::saveBans(const std::string& identifier, const Ban& banclass)
{
	Database* db = Database::instance();
	DBQuery query;
	
	if(!db->connect()){
		return false;
	}

	DBTransaction trans(db);
	if(!trans.start())
		return false;

	query << "DELETE FROM bans;";
	if(!db->executeQuery(query))
		return false;

	uint32_t currentTime = std::time(NULL);
	//save ip bans
	bool executeQuery = false;

	std::stringstream bans;

	DBSplitInsert query_insert(db);
	
	query_insert.setQuery("INSERT INTO `bans` (`type` , `ip` , `mask`, `time`) VALUES ");
	
	for(IpBanList::const_iterator it = banclass.ipBanList.begin(); it !=  banclass.ipBanList.end(); ++it){
		if(it->time > currentTime){
			executeQuery = true;
			bans << "(1," << it->ip << "," << it->mask <<
				"," << it->time << ")";

			if(!query_insert.addRow(bans.str()))
				return false;

			bans.str("");
		}
	}
	if(executeQuery){
		if(!query_insert.executeQuery())
			return false;
	}

	//save player bans
	executeQuery = false;
	query_insert.setQuery("INSERT INTO `bans` (`type` , `player` , `time`) VALUES ");
	
	for(PlayerBanList::const_iterator it = banclass.playerBanList.begin(); it !=  banclass.playerBanList.end(); ++it){
		if(it->time > currentTime){
			executeQuery = true;
			bans  << "(2," << it->id << "," << it->time << ")";
			
			if(!query_insert.addRow(bans.str()))
				return false;
			
			bans.str("");
		}
	}
	if(executeQuery){
		if(!query_insert.executeQuery())
			return false;
	}

	//save account bans
	executeQuery = false;
	query_insert.setQuery("INSERT INTO `bans` (`type` , `account` , `time`) VALUES ");
	
	for(AccountBanList::const_iterator it = banclass.accountBanList.begin(); it != banclass.accountBanList.end(); ++it){
		if(it->time > currentTime){
			executeQuery = true;
			bans << "(3," << it->id << "," << it->time << ")";
			
			if(!query_insert.addRow(bans.str()))
				return false;
			
			bans.str("");
		}
	}
	
	if(executeQuery){
		if(!query_insert.executeQuery())
			return false;
	}

	return trans.success();
}

#else

IOBanXML::IOBanXML()
{
	//
}

bool IOBanXML::loadBans(const std::string& identifier, Ban& banclass)
{
	xmlDocPtr doc = xmlParseFile(identifier.c_str());
	if(doc){
		xmlNodePtr root;
		root = xmlDocGetRootElement(doc);

		if(xmlStrcmp(root->name,(const xmlChar*)"bans") != 0){
			xmlFreeDoc(doc);
			return false;
		}

		xmlNodePtr banNode = root->children;
		while(banNode){
			if(xmlStrcmp(banNode->name,(const xmlChar*)"ban") == 0){

				int banType;
				if(readXMLInteger(banNode, "type", banType) && banType >= BAN_IPADDRESS && banType <= BAN_ACCOUNT){
					int time = 0;
					readXMLInteger(banNode, "time", time);

					switch(banType){
						case BAN_IPADDRESS:
						{
							int ip = 0;
							int mask = 0;

							if(readXMLInteger(banNode, "ip", ip)){

								readXMLInteger(banNode, "mask", mask);
								banclass.addIpBan(ip, mask, time);
							}

							break;
						}

						case BAN_PLAYER:
						{
							int playerguid = 0;
							if(readXMLInteger(banNode, "player", playerguid)){
								banclass.addPlayerBan(playerguid, time);
							}

							break;
						}

						case BAN_ACCOUNT:
						{
							int account = 0;
							if(readXMLInteger(banNode, "account", account)){
								banclass.addAccountBan(account, time);
							}

							break;
						}
					}
				}
				else{
					std::cout << "Warning: [IOBanXML::loadBans] could not load ban" << std::endl;
				}
			}

			banNode = banNode->next;
		}
	}

	return true;
}

bool IOBanXML::saveBans(const std::string& identifier, const Ban& banclass)
{
	xmlDocPtr doc = xmlNewDoc((xmlChar*) "1.0");
  xmlNodePtr nodeBans = xmlNewNode(NULL, (xmlChar*) "bans");
  xmlDocSetRootElement(doc, nodeBans);

	uint32_t currentTime = std::time(NULL);

	//save ip bans
	for(IpBanList::const_iterator it = banclass.ipBanList.begin(); it !=  banclass.ipBanList.end(); ++it){
		if(it->time > currentTime){

			xmlNodePtr nodeBan = xmlNewChild(nodeBans, NULL, (xmlChar*) "ban", NULL);

			std::stringstream ss;
			ss.str("");
			ss << (int) BAN_IPADDRESS;
			xmlNewProp(nodeBan, (xmlChar*) "type", (xmlChar*) ss.str().c_str());

			ss.str("");
			ss << (int) it->ip;
			xmlNewProp(nodeBan, (xmlChar*) "ip", (xmlChar*) ss.str().c_str());

			ss.str("");
			ss << (int) it->mask;
			xmlNewProp(nodeBan, (xmlChar*) "mask", (xmlChar*) ss.str().c_str());

			ss.str("");
			ss << (int) it->time;
			xmlNewProp(nodeBan, (xmlChar*) "time", (xmlChar*) ss.str().c_str());
		}
	}

	//save player bans
	for(PlayerBanList::const_iterator it = banclass.playerBanList.begin(); it !=  banclass.playerBanList.end(); ++it){
		if(it->time > currentTime){

			xmlNodePtr nodeBan = xmlNewChild(nodeBans, NULL, (xmlChar*) "ban", NULL);

			std::stringstream ss;
			ss.str("");
			ss << (int) BAN_PLAYER;
			xmlNewProp(nodeBan, (xmlChar*) "type", (xmlChar*) ss.str().c_str());

			ss.str("");
			ss << (int) it->id;
			xmlNewProp(nodeBan, (xmlChar*) "player", (xmlChar*) ss.str().c_str());

			ss.str("");
			ss << (int) it->time;
			xmlNewProp(nodeBan, (xmlChar*) "time", (xmlChar*) ss.str().c_str());
		}
	}

	for(AccountBanList::const_iterator it = banclass.accountBanList.begin(); it != banclass.accountBanList.end(); ++it){
		if(it->time > currentTime){
			xmlNodePtr nodeBan = xmlNewChild(nodeBans, NULL, (xmlChar*) "ban", NULL);

			std::stringstream ss;
			ss.str("");
			ss << (int) BAN_ACCOUNT;
			xmlNewProp(nodeBan, (xmlChar*) "type", (xmlChar*) ss.str().c_str());

			ss.str("");
			ss << (int) it->id;
			xmlNewProp(nodeBan, (xmlChar*) "account", (xmlChar*) ss.str().c_str());

			ss.str("");
			ss << (int) it->time;
			xmlNewProp(nodeBan, (xmlChar*) "time", (xmlChar*) ss.str().c_str());
		}
	}

	xmlSaveFormatFileEnc(identifier.c_str(), doc, "UTF-8", 1);
	xmlFreeDoc(doc);

	return true;
}

#endif

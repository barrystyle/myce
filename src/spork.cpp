// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <validation.h>
#include <messagesigner.h>
#include <net_processing.h>
#include <spork.h>
#include <netmessagemaker.h>

#include <boost/lexical_cast.hpp>

class CSporkMessage;
class CSporkManager;

CSporkManager sporkManager;

std::map<uint256, CSporkMessage> mapSporks;

namespace Spork {
static const int64_t SPORK_2_SWIFTTX_DEFAULT = 978307200;
static const int64_t SPORK_3_SWIFTTX_BLOCK_FILTERING_DEFAULT = 1424217600;
static const int64_t SPORK_5_MAX_VALUE_DEFAULT = 1000;
static const int64_t SPORK_7_MASTERNODE_SCANNING_DEFAULT = 978307200;
static const int64_t SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT_DEFAULT = 4070908800; 
static const int64_t SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT_DEFAULT = 4070908800;
static const int64_t SPORK_10_MASTERNODE_PAY_UPDATED_NODES_DEFAULT = 4070908800;
static const int64_t SPORK_13_ENABLE_SUPERBLOCKS_DEFAULT = 4070908800;
static const int64_t SPORK_14_NEW_PROTOCOL_ENFORCEMENT_DEFAULT = 4070908800;
static const int64_t SPORK_15_NEW_PROTOCOL_ENFORCEMENT_2_DEFAULT = 4070908800;
static const int64_t SPORK_16_ZEROCOIN_MAINTENANCE_MODE_DEFAULT = 4070908800;
}

void CSporkManager::ProcessSpork(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv, CConnman *connman)
{
    if (strCommand == NetMsgType::SPORK) {

        CSporkMessage spork;
        vRecv >> spork;

        uint256 hash = spork.GetHash();

        std::string strLogMsg;
        {
            LOCK(cs_main);
            pfrom->setAskFor.erase(hash);
            if(!chainActive.Tip()) return;
            strLogMsg = strprintf("SPORK -- hash: %s id: %d value: %10d bestHeight: %d peer=%d",
                                  hash.ToString(), spork.nSporkID,
                                  spork.nValue, chainActive.Height(),
                                  pfrom->GetId());
        }

        if(mapSporksActive.count(spork.nSporkID)) {
            if (mapSporksActive[spork.nSporkID].nTimeSigned >= spork.nTimeSigned) {
                LogPrint(BCLog::SPORK, "%s seen\n", strLogMsg);
                return;
            } else {
                LogPrint(BCLog::SPORK, "%s updated\n", strLogMsg);
            }
        } else {
            LogPrintf("%s %s new\n", __func__, strLogMsg);
        }

        if(!spork.CheckSignature()) {
            LogPrint(BCLog::SPORK, "ProcessSpork -- invalid signature\n");
            //Misbehaving(pfrom->GetId(), 100);
            return;
        }

        mapSporks[hash] = spork;
        mapSporksActive[spork.nSporkID] = spork;
        spork.Relay(connman);

        //does a task if needed
        ExecuteSpork(spork.nSporkID, spork.nValue);

    } else if (strCommand == NetMsgType::GETSPORKS) {

        std::map<int, CSporkMessage>::iterator it = mapSporksActive.begin();

        const CNetMsgMaker msgMaker(pfrom->GetSendVersion());

        while(it != mapSporksActive.end()) {
            connman->PushMessage(pfrom, msgMaker.Make(NetMsgType::SPORK, it->second));
            it++;
        }
    }

}

void CSporkManager::ExecuteSpork(int nSporkID, int nValue)
{
}

bool CSporkManager::UpdateSpork(int nSporkID, int64_t nValue, CConnman *connman)
{

    CSporkMessage spork = CSporkMessage(nSporkID, nValue, GetAdjustedTime());

    if(spork.Sign(strMasterPrivKey)) {
        spork.Relay(connman);
        mapSporks[spork.GetHash()] = spork;
        mapSporksActive[nSporkID] = spork;
        return true;
    }

    return false;
}

// grab the spork, otherwise say it's off
bool CSporkManager::IsSporkActive(int nSporkID)
{
    int64_t r = -1;

    if(mapSporksActive.count(nSporkID)){
        r = mapSporksActive[nSporkID].nValue;
    } else {
        using namespace Spork;
        switch (nSporkID) {
        case SPORK_2_SWIFTTX: r = SPORK_2_SWIFTTX_DEFAULT;
        case SPORK_3_SWIFTTX_BLOCK_FILTERING: r = SPORK_3_SWIFTTX_BLOCK_FILTERING_DEFAULT;
        case SPORK_5_MAX_VALUE: r = SPORK_5_MAX_VALUE_DEFAULT;
        case SPORK_7_MASTERNODE_SCANNING: r = SPORK_7_MASTERNODE_SCANNING_DEFAULT;
        case SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT: r = SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT_DEFAULT;
        case SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT: r = SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT_DEFAULT;
        case SPORK_10_MASTERNODE_PAY_UPDATED_NODES: r = SPORK_10_MASTERNODE_PAY_UPDATED_NODES_DEFAULT;
        case SPORK_13_ENABLE_SUPERBLOCKS: r = SPORK_13_ENABLE_SUPERBLOCKS_DEFAULT;
        case SPORK_14_NEW_PROTOCOL_ENFORCEMENT: r = SPORK_14_NEW_PROTOCOL_ENFORCEMENT_DEFAULT;
        case SPORK_15_NEW_PROTOCOL_ENFORCEMENT_2: r = SPORK_15_NEW_PROTOCOL_ENFORCEMENT_2_DEFAULT;
        case SPORK_16_ZEROCOIN_MAINTENANCE_MODE: r = SPORK_16_ZEROCOIN_MAINTENANCE_MODE_DEFAULT;
        default:
            LogPrint(BCLog::SPORK, "CSporkManager::IsSporkActive -- Unknown Spork ID %d\n", nSporkID);
            r = 4070908800ULL; // 2099-1-1 i.e. off by default
            break;
        }
    }

    return r < GetAdjustedTime();
}

// grab the value of the spork on the network, or the default
int64_t CSporkManager::GetSporkValue(int nSporkID)
{
    if (mapSporksActive.count(nSporkID))
        return mapSporksActive[nSporkID].nValue;

    using namespace Spork;

    switch (nSporkID) {
        case SPORK_2_SWIFTTX: return SPORK_2_SWIFTTX_DEFAULT;
        case SPORK_3_SWIFTTX_BLOCK_FILTERING: return SPORK_3_SWIFTTX_BLOCK_FILTERING_DEFAULT;
        case SPORK_5_MAX_VALUE: return SPORK_5_MAX_VALUE_DEFAULT;
        case SPORK_7_MASTERNODE_SCANNING: return SPORK_7_MASTERNODE_SCANNING_DEFAULT;
        case SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT: return SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT_DEFAULT;
        case SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT: return SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT_DEFAULT;
        case SPORK_10_MASTERNODE_PAY_UPDATED_NODES: return SPORK_10_MASTERNODE_PAY_UPDATED_NODES_DEFAULT;
        case SPORK_13_ENABLE_SUPERBLOCKS: return SPORK_13_ENABLE_SUPERBLOCKS_DEFAULT;
        case SPORK_14_NEW_PROTOCOL_ENFORCEMENT: return SPORK_14_NEW_PROTOCOL_ENFORCEMENT_DEFAULT;
        case SPORK_15_NEW_PROTOCOL_ENFORCEMENT_2: return SPORK_15_NEW_PROTOCOL_ENFORCEMENT_2_DEFAULT;
        case SPORK_16_ZEROCOIN_MAINTENANCE_MODE: return SPORK_16_ZEROCOIN_MAINTENANCE_MODE_DEFAULT;
        default:
           LogPrint(BCLog::SPORK, "CSporkManager::GetSporkValue -- Unknown Spork ID %d\n", nSporkID);
           return -1;
    }

}

int CSporkManager::GetSporkIDByName(std::string strName)
{
    using namespace Spork;
    if (strName == "SPORK_2_SWIFTTX") return SPORK_2_SWIFTTX;
    if (strName == "SPORK_3_SWIFTTX_BLOCK_FILTERING") return SPORK_3_SWIFTTX_BLOCK_FILTERING;
    if (strName == "SPORK_5_MAX_VALUE") return SPORK_5_MAX_VALUE;
    if (strName == "SPORK_7_MASTERNODE_SCANNING") return SPORK_7_MASTERNODE_SCANNING;
    if (strName == "SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT") return SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT;
    if (strName == "SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT") return SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT;
    if (strName == "SPORK_10_MASTERNODE_PAY_UPDATED_NODES") return SPORK_10_MASTERNODE_PAY_UPDATED_NODES;
    if (strName == "SPORK_13_ENABLE_SUPERBLOCKS") return SPORK_13_ENABLE_SUPERBLOCKS;
    if (strName == "SPORK_14_NEW_PROTOCOL_ENFORCEMENT") return SPORK_14_NEW_PROTOCOL_ENFORCEMENT;
    if (strName == "SPORK_15_NEW_PROTOCOL_ENFORCEMENT_2") return SPORK_15_NEW_PROTOCOL_ENFORCEMENT_2;
    if (strName == "SPORK_16_ZEROCOIN_MAINTENANCE_MODE") return SPORK_16_ZEROCOIN_MAINTENANCE_MODE;

    LogPrint(BCLog::SPORK, "CSporkManager::GetSporkIDByName -- Unknown Spork name '%s'\n", strName);
    return -1;
}

std::string CSporkManager::GetSporkNameByID(int id)
{
    using namespace Spork;
    if (id == SPORK_2_SWIFTTX) return "SPORK_2_SWIFTTX";
    if (id == SPORK_3_SWIFTTX_BLOCK_FILTERING) return "SPORK_3_SWIFTTX_BLOCK_FILTERING";
    if (id == SPORK_5_MAX_VALUE) return "SPORK_5_MAX_VALUE";
    if (id == SPORK_7_MASTERNODE_SCANNING) return "SPORK_7_MASTERNODE_SCANNING";
    if (id == SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT) return "SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT";
    if (id == SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT) return "SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT";
    if (id == SPORK_10_MASTERNODE_PAY_UPDATED_NODES) return "SPORK_10_MASTERNODE_PAY_UPDATED_NODES";
    if (id == SPORK_13_ENABLE_SUPERBLOCKS) return "SPORK_13_ENABLE_SUPERBLOCKS";
    if (id == SPORK_14_NEW_PROTOCOL_ENFORCEMENT) return "SPORK_14_NEW_PROTOCOL_ENFORCEMENT";
    if (id == SPORK_15_NEW_PROTOCOL_ENFORCEMENT_2) return "SPORK_15_NEW_PROTOCOL_ENFORCEMENT_2";
    if (id == SPORK_16_ZEROCOIN_MAINTENANCE_MODE) return "SPORK_16_ZEROCOIN_MAINTENANCE_MODE";
    return "Unknown";
}

bool CSporkManager::SetPrivKey(std::string strPrivKey)
{
    CSporkMessage spork;

    spork.Sign(strPrivKey);

    if(spork.CheckSignature()){
        // Test signing successful, proceed
        LogPrintf("CSporkManager::SetPrivKey -- Successfully initialized as spork signer\n");
        strMasterPrivKey = strPrivKey;
        return true;
    } else {
        return false;
    }
}

bool CSporkMessage::Sign(std::string strSignKey)
{
    CKey key;
    CPubKey pubkey;
    std::string strError = "";
    std::string strMessage = boost::lexical_cast<std::string>(nSporkID) + boost::lexical_cast<std::string>(nValue) + boost::lexical_cast<std::string>(nTimeSigned);

    if(!CMessageSigner::GetKeysFromSecret(strSignKey, key, pubkey)) {
        LogPrintf("CSporkMessage::Sign -- GetKeysFromSecret() failed, invalid spork key %s\n", strSignKey);
        return false;
    }

    if(!CMessageSigner::SignMessage(strMessage, vchSig, key, CPubKey::InputScriptType::SPENDP2PKH)) {
        LogPrintf("CSporkMessage::Sign -- SignMessage() failed\n");
        return false;
    }

    if(!CMessageSigner::VerifyMessage(pubkey.GetID(), vchSig, strMessage, strError)) {
        LogPrintf("CSporkMessage::Sign -- VerifyMessage() failed, error: %s\n", strError);
        return false;
    }

    return true;
}

bool CSporkMessage::CheckSignature()
{
    //note: need to investigate why this is failing
    std::string strError = "";
    std::string strMessage = boost::lexical_cast<std::string>(nSporkID) + boost::lexical_cast<std::string>(nValue) + boost::lexical_cast<std::string>(nTimeSigned);
    CPubKey pubkey(GetTime() > 1559844000 ? ParseHex(Params().SporkPubKey()) : ParseHex(Params().SporkPubKeyOld()));

    if(!CMessageSigner::VerifyMessage(pubkey.GetID(), vchSig, strMessage, strError)) {
        LogPrintf("%s failed, error: %s\n", __func__, strError);
        return false;
    }

    return true;
}

void CSporkMessage::Relay(CConnman *connman)
{
    CInv inv(MSG_SPORK, GetHash());
    connman->ForEachNode([&inv](CNode* pnode)
    {
        pnode->PushInventory(inv);
    });
}

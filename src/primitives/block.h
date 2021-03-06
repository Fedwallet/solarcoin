// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_PRIMITIVES_BLOCK_H
#define BITCOIN_PRIMITIVES_BLOCK_H

#include <hash.h>
#include <crypto/common.h>
#include <crypto/scrypt.h>
#include <primitives/transaction.h>
#include <serialize.h>
#include <uint256.h>
#include <util.h>
#include <utilstrencodings.h>

/** Nodes collect new transactions into a block, hash them into a hash tree,
 * and scan through nonce values to make the block's hash satisfy proof-of-work
 * requirements.  When they solve the proof-of-work, they broadcast the block
 * to everyone and the block is added to the block chain.  The first transaction
 * in the block is a special one that creates a new coin owned by the creator
 * of the block.
 */
class CBlockHeader
{
public:
    // header
    static const int LEGACY_VERSION_1 = 1;
    static const int LEGACY_VERSION_2 = 2;
    static const int CURRENT_VERSION = 3;
    int32_t nVersion;
    uint256 hashPrevBlock;
    uint256 hashMerkleRoot;
    uint32_t nTime;
    uint32_t nBits;
    uint32_t nNonce;

    CBlockHeader()
    {
        SetNull();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(this->nVersion);
        READWRITE(hashPrevBlock);
        READWRITE(hashMerkleRoot);
        READWRITE(nTime);
        READWRITE(nBits);
        READWRITE(nNonce);
    }

    void SetNull()
    {
        nVersion = 0;
        hashPrevBlock.SetNull();
        hashMerkleRoot.SetNull();
        nTime = 0;
        nBits = 0;
        nNonce = 0;
    }

    bool IsNull() const
    {
        return (nBits == 0);
    }

    uint256 GetPoWHash() const
    {
        uint256 thash;
        scrypt_1024_1_1_256(BEGIN(nVersion), BEGIN(thash));
        return thash;
    }

    uint256 GetHash() const
    {
        //return SerializeHash(*this);
        return Hash(BEGIN(nVersion), END(nNonce));
    }

    int64_t GetBlockTime() const
    {
        return (int64_t)nTime;
    }
};


class CBlock : public CBlockHeader
{
public:
    // network and disk
    std::vector<CTransactionRef> vtx;

    // ppcoin: block signature - signed by one of the coin base txout[N]'s owner
    std::vector<unsigned char> vchBlockSig;
    
    // memory only
    mutable bool fChecked;

    CBlock()
    {
        SetNull();
    }

    CBlock(const CBlockHeader &header)
    {
        SetNull();
        *((CBlockHeader*)this) = header;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(*(CBlockHeader*)this);
        // PoST: ConnectBlock depends on vtx following header to generate CDiskTxPos
        if (!(s.GetType() & (SER_GETHASH|SER_BLOCKHEADERONLY))) {
            READWRITE(vtx);
            if (this->nVersion >= CBlockHeader::CURRENT_VERSION) {
                READWRITE(vchBlockSig);
            }
        } else {
            if (ser_action.ForRead())
            {
                const_cast<CBlock*>(this)->vtx.clear();
                const_cast<CBlock*>(this)->vchBlockSig.clear();
            }
        }
    }

    void SetNull()
    {
        CBlockHeader::SetNull();
        vtx.clear();
        vchBlockSig.clear();
        fChecked = false;
    }

    CBlockHeader GetBlockHeader() const
    {
        CBlockHeader block;
        block.nVersion       = nVersion;
        block.hashPrevBlock  = hashPrevBlock;
        block.hashMerkleRoot = hashMerkleRoot;
        block.nTime          = nTime;
        block.nBits          = nBits;
        block.nNonce         = nNonce;
        return block;
    }

    std::string ToString() const;

    /* SolarCoin methods */

    // ppcoin: entropy bit for stake modifier if chosen by modifier
    unsigned int GetStakeEntropyBit(unsigned int nTime) const
    {
        // Take last bit of block hash as entropy bit
        unsigned int nEntropyBit = ((GetHash().GetUint64(0)) & 1llu);
        if (gArgs.GetBoolArg("-printstakemodifier", false))
            LogPrintf("GetStakeEntropyBit: nTime=%u hashBlock=%s nEntropyBit=%u\n", nTime, GetHash().ToString().c_str(), nEntropyBit);
        return nEntropyBit;
    }

    // ppcoin: two types of block: proof-of-work or proof-of-stake
    bool IsProofOfStake() const
    {
        const CTransaction& tx = *vtx[1];
        return (vtx.size() > 1 && tx.IsCoinStake());
    }

    bool IsProofOfWork() const
    {
        return !IsProofOfStake();
    }

    std::pair<COutPoint, unsigned int> GetProofOfStake() const
    {
        const CTransaction& tx = *vtx[1];
        return IsProofOfStake() ? std::make_pair(tx.vin[0].prevout, tx.nTime) : std::make_pair(COutPoint(), (unsigned int)0);
    }
};

/** Describes a place in the block chain to another node such that if the
 * other node doesn't have the same branch, it can find a recent common trunk.
 * The further back it is, the further before the fork it may be.
 */
struct CBlockLocator
{
    std::vector<uint256> vHave;

    CBlockLocator() {}

    explicit CBlockLocator(const std::vector<uint256>& vHaveIn) : vHave(vHaveIn) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vHave);
    }

    void SetNull()
    {
        vHave.clear();
    }

    bool IsNull() const
    {
        return vHave.empty();
    }
};

#endif // BITCOIN_PRIMITIVES_BLOCK_H

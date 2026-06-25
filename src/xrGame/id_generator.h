////////////////////////////////////////////////////////////////////////////
//	Module 		: id_generator.h
//	Created 	: 28.08.2003
//  Modified 	: 28.08.2003
//	Author		: Dmitriy Iassenev and Oles' Shyshkovtsov
//	Description : ID generation class template
////////////////////////////////////////////////////////////////////////////

#pragma once

template <
	typename TIME_ID,
	typename TYPE_ID,
	typename VALUE_ID,
	typename BLOCK_ID,
	typename CHUNK_ID,
	VALUE_ID tMinValue,
	VALUE_ID tMaxValue,
	CHUNK_ID tBlockSize,
	VALUE_ID tInvalidValueID = tMaxValue,
	TIME_ID tStartTime = 0>
class CID_Generator
{
private:
	struct SID_Block
	{
		CHUNK_ID m_tCount;
		TIME_ID m_tTimeID;
		TYPE_ID m_tpIDs[tBlockSize];

		IC SID_Block() : m_tCount(0)
		{
		}

		IC bool operator<(const SID_Block& b) const
		{
			return (m_tCount && ((m_tTimeID < b.m_tTimeID) || !b.m_tCount));
		}
	};

private:
	u32 m_available_count;
	VALUE_ID m_tNextValue;
	bool m_has_unassigned_ids;
	xr_hash_map<BLOCK_ID, SID_Block> m_tppBlocks;
	xr_unordered_set<VALUE_ID> m_tReservedIDs;

private:
	IC BLOCK_ID tfGetBlockByValue(VALUE_ID tValueID)
	{
		R_ASSERT2(tValueID >= tMinValue && tValueID <= tMaxValue, "Requesting ID is invalid!");
		BLOCK_ID l_tBlockID = BLOCK_ID((tValueID - tMinValue) / tBlockSize);
		return (l_tBlockID);
	}

	IC void tfAdvanceNextValue()
	{
		if (!m_has_unassigned_ids)
			return;

		if (m_tNextValue >= tMaxValue)
		{
			m_has_unassigned_ids = false;
			return;
		}

		++m_tNextValue;
	}

	IC void tfSkipReservedIDs()
	{
		while (m_has_unassigned_ids)
		{
			auto I = m_tReservedIDs.find(m_tNextValue);
			if (I == m_tReservedIDs.end())
				return;

			m_tReservedIDs.erase(I);
			tfAdvanceNextValue();
		}
	}

	IC VALUE_ID tfGetFromBlock(SID_Block& l_tID_Block, BLOCK_ID l_tBlockID, VALUE_ID tValueID)
	{
		VERIFY(l_tID_Block.m_tCount);

		if (l_tID_Block.m_tCount == 1)
		{
			--m_available_count;
			VERIFY(m_available_count >= 0);
		}

		if (tInvalidValueID == tValueID)
		{
			const VALUE_ID l_tResult =
			    VALUE_ID(l_tID_Block.m_tpIDs[--l_tID_Block.m_tCount]) + l_tBlockID * tBlockSize + tMinValue;
			if (!l_tID_Block.m_tCount)
				m_tppBlocks.erase(l_tBlockID);
			return l_tResult;
		}

		TYPE_ID* l_tpBlockID = std::find(l_tID_Block.m_tpIDs, l_tID_Block.m_tpIDs + l_tID_Block.m_tCount,
		                                 TYPE_ID((tValueID - tMinValue) % tBlockSize));
		R_ASSERT2(l_tID_Block.m_tpIDs + l_tID_Block.m_tCount != l_tpBlockID, "Requesting ID has already been used!");
		*l_tpBlockID = *(l_tID_Block.m_tpIDs + --l_tID_Block.m_tCount);
		if (!l_tID_Block.m_tCount)
			m_tppBlocks.erase(l_tBlockID);
		return (tValueID);
	}

public:
	IC CID_Generator()
	{
		m_available_count = 0;
		m_tNextValue = tMinValue;
		m_has_unassigned_ids = true;
		m_tppBlocks.clear();
		m_tReservedIDs.clear();
	}

	IC VALUE_ID tfGetID(VALUE_ID tValueID = tInvalidValueID)
	{
		if (tInvalidValueID != tValueID)
		{
			BLOCK_ID l_tBlockID = tfGetBlockByValue(tValueID);
			auto I = m_tppBlocks.find(l_tBlockID);
			if (I != m_tppBlocks.end())
				return (tfGetFromBlock(I->second, l_tBlockID, tValueID));

			if (m_has_unassigned_ids && tValueID >= m_tNextValue)
			{
				if (tValueID == m_tNextValue)
				{
					tfAdvanceNextValue();
					tfSkipReservedIDs();
					return tValueID;
				}

				auto inserted = m_tReservedIDs.insert(tValueID);
				R_ASSERT2(inserted.second, "Requesting ID has already been used!");
				return tValueID;
			}

			R_ASSERT2(false, "Requesting ID has already been used!");
			return tInvalidValueID;
		}

		auto I = m_tppBlocks.end();
		if (!m_tppBlocks.empty())
		{
			I = std::min_element(m_tppBlocks.begin(), m_tppBlocks.end(), [](const auto& left, const auto& right) {
				return left.second < right.second;
			});
		}

		if (m_has_unassigned_ids && (I == m_tppBlocks.end() || tStartTime <= I->second.m_tTimeID))
		{
			VALUE_ID l_tResult = m_tNextValue;
			tfAdvanceNextValue();
			tfSkipReservedIDs();
			return l_tResult;
		}

		R_ASSERT2(I != m_tppBlocks.end(), "Not enough IDs");
		return (tfGetFromBlock(I->second, I->first, tValueID));
	}

	IC void vfFreeID(VALUE_ID tValueID, TIME_ID tTimeID)
	{
		BLOCK_ID l_tBlockID = tfGetBlockByValue(tValueID);
		SID_Block& l_tID_Block = m_tppBlocks[l_tBlockID];

		VERIFY(l_tID_Block.m_tCount < tBlockSize);

		if (!l_tID_Block.m_tCount)
		{
			++m_available_count;
			VERIFY(m_available_count <= m_tBlockCount);
		}

#ifdef DEBUG
		TYPE_ID					*l_tpBlockID = std::find(l_tID_Block.m_tpIDs, l_tID_Block.m_tpIDs + l_tID_Block.m_tCount, TYPE_ID((tValueID - tMinValue)%tBlockSize));	
		VERIFY					(l_tpBlockID == l_tID_Block.m_tpIDs + l_tID_Block.m_tCount);
#endif
		if (m_has_unassigned_ids && tValueID > m_tNextValue)
		{
			const size_t l_tErased = m_tReservedIDs.erase(tValueID);
			VERIFY(l_tErased);
		}
		l_tID_Block.m_tpIDs[l_tID_Block.m_tCount++] = TYPE_ID((tValueID - tMinValue) % tBlockSize);
		l_tID_Block.m_tTimeID = tTimeID;
	}
};

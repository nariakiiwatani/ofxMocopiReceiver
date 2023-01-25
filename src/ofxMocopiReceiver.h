#pragma once

#include "ofxUDPManager.h"
#include <cstdint>
#include "ofNode.h"
#include "ofEventUtils.h"

namespace {
template<typename T>
T get(const char *data, bool swap_endian=false) {
	std::size_t data_size = sizeof(T);
	if(swap_endian) {
		std::string buf(data_size, 0);
		auto dst = const_cast<char*>(buf.data());
		for(int i = 0; i < data_size; ++i) {
			dst[data_size-1-i] = data[i];
		}
		return get<T>(dst, false);
	}
	return *reinterpret_cast<typename std::add_const<T>::type*>(data);
}
}

namespace ofx { namespace mocopi {
class Reader
{
public:
	virtual bool isAcceptableChunk(std::string name, std::size_t length) const { return isAcceptableChunk(name); }
	virtual bool isAcceptableChunk(std::string name) const { return ofContains(acceptable_chunk_names_, name); }
	void setAcceptableChunkNames(std::vector<std::string> names) { acceptable_chunk_names_ = names; }
	void read(const std::string &data) { read(data.data(), data.size()); }
	int read(const char *data, std::size_t length) {
		auto ptr = data;
		while(length >= 8) {
			auto &&chunk_length = get<uint32_t>(ptr);
			if(chunk_length == 0) {
				break;
			}
			std::string chunk_name(ptr+4, 4);

			ptr += 8;
			length -= 8;
						
			if(isAcceptableChunk(chunk_name, chunk_length)) {
				ofNotifyEvent(will_accept_[chunk_name], this);
				accept(chunk_name, ptr, chunk_length);
				ofNotifyEvent(did_accept_[chunk_name], this);
			}
			
			length -= chunk_length;
			ptr += chunk_length;
		}
		return (int)(ptr - data);
	}
	void accept(std::string chunk_name, const char *data, std::size_t length) {
		decode(chunk_name, data, length);
		auto range = reader_.equal_range(chunk_name);
		for(auto it = range.first; it != range.second; ++it) {
			it->second->read(data, length);
		}
	}
	virtual void decode(std::string chunk_name, const char *data, std::size_t length) {}
	
	void addReader(std::string parent_chunk_name, std::shared_ptr<Reader> reader) {
		reader_.insert({parent_chunk_name, reader});
	}
	void removeReader(std::string parent_chunk_name, std::shared_ptr<Reader> reader) {
		auto range = reader_.equal_range(parent_chunk_name);
		for(auto it = range.first; it != range.second; ++it) {
			if(it->second == reader) {
				reader_.erase(it);
				return;
			}
		}
		ofLogWarning("ofxMocopiReceiver") << "reader not found";
		return;
	}

	std::map<std::string, ofEvent<void>> will_accept_, did_accept_;

private:
	std::multimap<std::string, std::shared_ptr<Reader>> reader_;
	std::vector<std::string> acceptable_chunk_names_;
};

template<typename R>
std::shared_ptr<R> createReader(std::vector<std::string> acceptable) {
	auto ret = std::make_shared<R>();
	ret->setAcceptableChunkNames(acceptable);
	return ret;
}
template<typename R>
std::shared_ptr<R> createReader(std::vector<std::string> acceptable, std::vector<std::pair<std::shared_ptr<ofx::mocopi::Reader>, std::string>> parents) {
	auto ret = createReader<R>(acceptable);
	for(auto &&p : parents) {
		p.first->addReader(p.second, ret);
	}
	return ret;
}
template<typename R>
std::shared_ptr<R> createReader(std::vector<std::string> acceptable, std::vector<std::pair<ofx::mocopi::Reader*, std::string>> parents) {
	auto ret = createReader<R>(acceptable);
	for(auto &&p : parents) {
		p.first->addReader(p.second, ret);
	}
	return ret;
}

class RawCopyReader : public Reader
{
public:
	void decode(std::string chunk_name, const char *data, std::size_t length) override {
		data_ = {data, length};
	}
	template<typename T> explicit operator T&() noexcept { return *(T*)(const_cast<char*>(data_.data())); }
	template<typename T> explicit operator const T&() const noexcept { return *(const T*)(data_.data()); }
	template<typename T> explicit operator T*() noexcept { return (T*)(const_cast<char*>(data_.data())); }
	template<typename T> explicit operator const T*() const noexcept { return (const T*)(data_.data()); }
private:
	std::string data_;
};

class Receiver : public Reader
{
public:
	bool listen(uint16_t port) {
		if(socket_.Create() && socket_.Bind(port)) {
			socket_.SetNonBlocking(true);
			return true;
		}
		return false;
	}
	bool close() {
		return socket_.Close();
	}
	void update() {
		while(true) {
			std::string buf = getNextMessage();
			if(buf.size() == 0) {
				return;
			}
			read(buf);
		}
	}

private:
	ofxUDPManager socket_;
	
	std::string getNextMessage() {
		int available_length = socket_.PeekReceive();
		if(available_length == 0) {
			return {};
		}
		std::string buf(available_length, '\0');
		auto ptr = const_cast<char*>(buf.data());
		int received_length = socket_.Receive(ptr, available_length);
		if(received_length == 0 || !isValidPacket(ptr, received_length)) {
			ofLogNotice("ofx::mocopi::Reader") << "received invalid packet";
			return {};
		}
		buf.resize(received_length);
		return buf;
	}
	bool isValidPacket(const char *data, std::size_t length) const {
		int checked = 0;
		while(checked < length) {
			auto &&chunk_length = get<uint32_t>(data+checked);
			checked += chunk_length + 8;
			if(chunk_length == 0) {
				return false;
			}
		}
		return checked == length;
	}
};

class BoneReader : public Reader
{
public:
	static const std::size_t NUM_BONES = 27;
	static constexpr float SCENE_SCALE = 1000.f;
	BoneReader() {
		resetSkeleton();
		constructSkeleton();
		
		pbid_ = createReader<RawCopyReader>({"pbid"}, {{this, "bndt"}});
		bnid_ = createReader<RawCopyReader>({"bnid"}, {{this, "bndt"}, {this, "btdt"}});
		trans_ = createReader<RawCopyReader>({"tran"}, {{this, "bndt"}, {this, "btdt"}});
		
		ofAddListener(did_accept_["bndt"], this, &BoneReader::updateDefinition);
		ofAddListener(did_accept_["btdt"], this, &BoneReader::updateTransform);
	}

	void updateDefinition() {
		auto bnid = (uint16_t)(*bnid_);
		auto pbid = (uint16_t)(*pbid_);
		if(bnid < NUM_BONES && pbid < NUM_BONES) {
			bone_[bnid].setParent(bone_[pbid]);
		}
		updateTransform();
	}
	void updateTransform() {
		auto bnid = (uint16_t)(*bnid_);
		auto &bone = bone_[bnid];
		auto trans = (float*)(*trans_);
		float *o = trans;
		float *p = trans+4;
		bone.setPosition(p[0]*SCENE_SCALE, p[1]*SCENE_SCALE, p[2]*SCENE_SCALE);
		bone.setOrientation({o[3], o[0], o[1], o[2]});
	}
	const std::vector<ofNode>& getBones() const { return bone_; }
	void resetSkeleton() {
		bone_.resize(NUM_BONES);
		for(auto &&b : bone_) {
			b.clearParent();
		}
	}
private:
	void constructSkeleton() {
		auto do_index = [&](std::size_t c, std::size_t p) {
			bone_[c].setParent(bone_[p]);
		};
		auto do_array = [&](std::vector<std::size_t> indices) {
			for(int i = 0; i < indices.size()-1; ++i) {
				do_index(indices[i+1], indices[i]);
			}
		};
		do_array({0,1,2,3,4,5,6,7,8,9,10});
		do_array({7,11,12,13,14});
		do_array({7,15,16,17,18});
		do_array({7,19,20,21,22});
		do_array({7,23,24,25,26});
	}
	std::vector<ofNode> bone_;
	std::shared_ptr<RawCopyReader> bnid_, pbid_, trans_;
};
}}

class ofxMocopiReceiver
{
public:
	ofxMocopiReceiver() {
		using namespace ofx::mocopi;
		receiver_ = createReader<Receiver>({"head", "sndf", "skdf", "fram"});

		auto bons = createReader<Reader>({"bons"}, {{receiver_, "skdf"}});
		auto btrs = createReader<Reader>({"btrs"}, {{receiver_, "fram"}});
		bone_ = createReader<BoneReader>({"bndt", "btdt"}, {{bons, "bons"}, {btrs, "btrs"}});
		
		ftyp_ = createReader<RawCopyReader>({"ftyp"}, {{receiver_, "head"}});
		vrsn_ = createReader<RawCopyReader>({"vrsn"}, {{receiver_, "head"}});

		ipad_ = createReader<RawCopyReader>({"ipad"}, {{receiver_, "sndf"}});
		rcvp_ = createReader<RawCopyReader>({"rcvp"}, {{receiver_, "sndf"}});

		fnum_ = createReader<RawCopyReader>({"fnum"}, {{receiver_, "fram"}});
		time_ = createReader<RawCopyReader>({"time"}, {{receiver_, "fram"}});
		
		ofAddListener(bons->will_accept_["bons"], bone_.get(), &BoneReader::resetSkeleton);
	}
	bool setup(uint16_t port = 12351) {
		if(is_setup_) {
			receiver_->close();
		}
		port_ = port;
		return is_setup_ = receiver_->listen(port);
	}
	bool isSetup() const { return is_setup_; }
	uint16_t getPort() const { return port_; }
	void update() {
		receiver_->update();
	}
	struct Info {
		struct Head {
			std::string ftyp;
			char vrsn;
		} head;
		struct Sndf {
			char ipad[4];
			uint16_t rcvp;
		} sndf;
		struct Fram {
			uint32_t fnum;
			uint32_t time;
		} fram;
	};
	const std::vector<ofNode>& getBones() const { return bone_->getBones(); }
	Info getInfo() const {
		Info ret;
		ret.head.ftyp = (std::string)(*ftyp_);
		ret.head.vrsn = (char)(*vrsn_);
		memcpy(ret.sndf.ipad, (void*)(*ipad_), 4);
		ret.sndf.rcvp = (uint16_t)(*rcvp_);
		ret.fram.fnum = (uint32_t)(*fnum_);
		ret.fram.time = (uint32_t)(*time_);
		return ret;
	}
private:
	std::shared_ptr<ofx::mocopi::Receiver> receiver_;
	std::shared_ptr<ofx::mocopi::BoneReader> bone_;
	std::shared_ptr<ofx::mocopi::RawCopyReader> ftyp_, vrsn_, ipad_, rcvp_, fnum_, time_;
	bool is_setup_=false;
	uint16_t port_=0;
};

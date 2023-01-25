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
				accept(chunk_name, ptr, chunk_length);
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

private:
	std::multimap<std::string, std::shared_ptr<Reader>> reader_;
	std::vector<std::string> acceptable_chunk_names_;
};

class Receiver : public Reader
{
public:
	Receiver() {
		setAcceptableChunkNames({"head", "sndf", "skdf", "fram"});
	}
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
# pragma pack (1)
struct BtdtData {
	uint32_t bnid_size; // 2
	char bnid_name[4]; // bnid
	uint16_t bnid;
	
	uint32_t tran_size; // 28
	char tran_name[4]; // tran
	float orientation[4];
	float position[3];
};
# pragma pack ()
class BoneReader : public Reader
{
public:
	static const std::size_t NUM_BONES = 27;
	static constexpr float SCENE_SCALE = 1000.f;
	BoneReader() {
		constructSkeleton();
		setAcceptableChunkNames({"bndt", "btdt"});
	}
	void decode(std::string chunk_name, const char *data, std::size_t length) {
		if(chunk_name == "btdt") {
			const BtdtData &d = *(BtdtData*)(data);
			auto &bone = bone_[d.bnid];
			bone.setPosition(d.position[0]*SCENE_SCALE, d.position[1]*SCENE_SCALE, d.position[2]*SCENE_SCALE);
			bone.setOrientation({d.orientation[3], d.orientation[0], d.orientation[1], d.orientation[2]});
		}
	}
	const std::vector<ofNode>& getBones() const { return bone_; }
private:
	void constructSkeleton() {
		bone_.resize(NUM_BONES);
		for(auto &&b : bone_) {
			b.clearParent();
		}
		auto do_index = [&](std::size_t c, std::size_t p) {
			bone_[c].setParent(bone_[p]);
		};
		auto do_array = [&](std::vector<std::size_t> indices) {
			for(int i = 0; i < indices.size()-1; ++i) {
				do_index(indices[i+1], indices[i]);
			}
		};
		do_array({0,1,2,3,4,5,6,7,8,9,10});
		do_array({6,11,12,13,14});
		do_array({6,15,16,17,18});
		do_array({0,19,20,21,22});
		do_array({0,23,24,25,26});
	}
	std::vector<ofNode> bone_;
};
}}

class ofxMocopiReceiver
{
public:
	ofxMocopiReceiver() {
		using namespace ofx::mocopi;
		receiver_ = std::make_shared<Receiver>();
		auto bons = std::make_shared<Reader>();
		bons->setAcceptableChunkNames({"bons"});
		receiver_->addReader("skdf", bons);
		auto btrs = std::make_shared<Reader>();
		btrs->setAcceptableChunkNames({"btrs"});
		receiver_->addReader("fram", btrs);
		
		bone_ = std::make_shared<BoneReader>();
		bons->addReader("bons", bone_);
		btrs->addReader("btrs", bone_);
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
	const std::vector<ofNode>& getBones() const { return bone_->getBones(); }
private:
	std::shared_ptr<ofx::mocopi::Receiver> receiver_;
	std::shared_ptr<ofx::mocopi::BoneReader> bone_;
	bool is_setup_=false;
	uint16_t port_=0;
};

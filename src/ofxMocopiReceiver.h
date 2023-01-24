#pragma once

#include "ofxUDPManager.h"
#include <cstdint>
#include "ofNode.h"
#include "ofEventUtils.h"

namespace ofx { namespace mocopi {
class ReaderBase
{
public:
	virtual std::string getChunkName() const=0;
	std::size_t getChunkNameLength() const { return getChunkName().size(); }
	virtual std::size_t getDataLength() const=0;
	virtual void read(const void *data) {}
};
template<typename T>
class Reader : public ReaderBase
{
public:
	ofEvent<const T> onRead;
protected:
	void read(const void *data) override {
		ofNotifyEvent(onRead, *(const T*)data, this);
		read(*(const T*)data);
	}
	virtual void read(const T &data) {}
};
class Receiver
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
	void addReader(std::shared_ptr<ReaderBase> reader) {
		reader_.push_back(reader);
	}
	void removeReader(std::shared_ptr<ReaderBase> reader) {
		auto found = find(begin(reader_), end(reader_), reader);
		if(found == end(reader_)) {
			ofLogWarning("ofxMocopiReceiver") << "reader not found";
			return;
		}
		reader_.erase(found);
	}
	template<typename T> std::shared_ptr<T> addReader() {
		auto ret = std::make_shared<T>();
		addReader(ret);
		return ret;
	}
	void update() {
		int rest = socket_.PeekReceive();
		if(rest == 0) {
			return;
		}
		std::string buf(rest, '\0');
		auto ptr = const_cast<char*>(buf.data());
		while(rest > 0) {
			int length = socket_.Receive(ptr, rest);
			if(length == 0) {
				break;
			}
			ptr += length;
			rest -= length;
		}

		for(auto &&r : reader_) {
			for(std::string::size_type pos = 0; (pos = buf.find(r->getChunkName(), pos)) != string::npos; pos += r->getDataLength()) {
				r->read(buf.data()+pos);
			}
		}
	}

private:
	ofxUDPManager socket_;
	std::vector<std::shared_ptr<ReaderBase>> reader_;
};
# pragma pack (1)
struct BtdtData {
	char btdt_name[4]; // btdt
	char btdt[4];
	char bnid_name[4]; // bnid
	char bnid;
	char unknown1[5];
	char tran_name[4]; // tran
	float orientation[4];
	float position[3];
};
# pragma pack ()
class BoneReader : public Reader<BtdtData>
{
public:
	static const std::size_t NUM_BONES = 27;
	static constexpr float SCENE_SCALE = 1000.f;
	BoneReader() {
		constructSkeleton();
	}
	std::string getChunkName() const override { return "btdt"; }
	std::size_t getDataLength() const override { return sizeof(BtdtData); }
	void read(const BtdtData &d) override {
		auto &bone = bone_[d.bnid];
		bone.setPosition(d.position[0]*SCENE_SCALE, d.position[1]*SCENE_SCALE, d.position[2]*SCENE_SCALE);
		bone.setOrientation({d.orientation[3], d.orientation[0], d.orientation[1], d.orientation[2]});
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
		receiver_ = std::make_shared<ofx::mocopi::Receiver>();
		bone_ = receiver_->addReader<ofx::mocopi::BoneReader>();
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

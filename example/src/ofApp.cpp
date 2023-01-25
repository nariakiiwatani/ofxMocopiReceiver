#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){
	receiver_.setup();
	camera_.setDistance(5000);
}

//--------------------------------------------------------------
void ofApp::update(){
	if(receiver_.isSetup()) {
		receiver_.update();
	}
}

//--------------------------------------------------------------
void ofApp::draw(){
	camera_.begin();
	for(auto &&bone : receiver_.getBones()) {
		bone.draw();
		if(auto p = bone.getParent()) {
			ofDrawArrow(p->getGlobalPosition(), bone.getGlobalPosition(), 20);
		}
	}
	camera_.end();
	if(receiver_.isSetup()) {
		auto info = receiver_.getInfo();
		std::stringstream ss;
		ss << "ftyp: " << info.head.ftyp << std::endl;
		ss << "vrsn: " << (int)(info.head.vrsn) << std::endl;
		ss << "ipad: 0x" << ofToHex(std::string(info.sndf.ipad,4)) << std::endl;
		ss << "rcvp: " << info.sndf.rcvp << std::endl;
		ss << "fnum: " << info.fram.fnum << std::endl;
		ss << "time: " << info.fram.time;
		ofDrawBitmapStringHighlight(ss.str(), 0, 20);
	}
	else {
		ofDrawBitmapStringHighlight("receiver not bound. hit Enter to rebind", 0, 20);
	}
	
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
	switch(key) {
		case OF_KEY_RETURN:
			receiver_.setup();
			break;
	}

}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}

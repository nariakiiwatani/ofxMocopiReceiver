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
	}
	camera_.end();
	if(receiver_.isSetup()) {
		ofDrawBitmapStringHighlight("receiver bind port: " + ofToString(receiver_.getPort()), 0, 20);
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

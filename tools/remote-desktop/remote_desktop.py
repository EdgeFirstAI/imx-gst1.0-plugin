#!/usr/bin/env python3
# IMX pipewire recorder
# Copyright 2025 NXP
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

import os
import sys
import time
import fnmatch
import argparse
import datetime
import subprocess
import socket
import http.server
import signal
from enum import Enum
from enum import IntEnum

VERSION_STR = "REMOTE_DESKTOP_00.00.11"

# Return result
class Ret(Enum):
    OK = 0
    PARAM_CHANGE = 1
    ERROR = -1
    ERROR_FILE_NO_FOUND = -2

class WorkMode(IntEnum):
    CLIENT = 0
    SERVER = 1

class RecordType(IntEnum):
    RTSP = 0
    RTP = 1
    HLS = 2
    FILE = 3

class StreamPayloader(IntEnum):
    RTP_MP2_PAY = 0
    RTP_H264_PAY = 1

# Weston configuration base class
class WestonConfig:
    file_name = "/etc/xdg/weston/weston.ini"

    def set_section(self, section, param, is_enable):
        ret = Ret.OK
        section_keyword = ["[", "]"]

        try:
            with open(self.file_name, "r+", encoding="utf-8") as file:
                lines = file.readlines()
                file.seek(0)
                file.truncate()
                need_check = False

                for line in lines:
                    section_pos = line.find(section)
                    if section_pos >= 0:
                        need_check = True

                    if need_check:
                        param_pos = line.find(param)
                        if param_pos == 0:
                            if is_enable == False:
                                # Disable the param
                                line = "#" + line
                                need_check = False
                                print (f"INFO: Disable info: {param}")
                                ret = Ret.PARAM_CHANGE
                            else:
                                need_check = False
                        elif param_pos > 0:
                            # Found the param but not be enabled
                            line = line[param_pos:]
                            need_check = False
                            print (f"INFO: Found info: {param}")
                            ret = Ret.PARAM_CHANGE
                        elif section_pos == -1:
                            # Append the param at end of the selected section
                            left_section_pos = line.find(section_keyword[0])
                            right_section_pos = line.find(section_keyword[1])
                            if left_section_pos >= 0 and right_section_pos > 2:
                                line = param + line
                                need_check = False
                                print (f"INFO: Add info: {param}")
                                ret = Ret.PARAM_CHANGE

                    file.write (line)
                file.flush 
        except FileNotFoundError:
            print (f"Error: Fail to configure the file: {self.file_name}")
            ret = Ret.ERROR_FILE_NO_FOUND
        return ret
    def restart(self, time_sec):
        os.system ("systemctl restart weston")
        if time_sec:
            time.sleep(time_sec)

# Configure libinput
class Libinput(WestonConfig):
    input_section = "[libinput]"
    disable_libinput = "disable-input=true\n"

    def disable(self):
        ret = super().set_section(self.input_section, self.disable_libinput, True)
        if ret == Ret.PARAM_CHANGE:
            super().restart(2)
            print ("INFO: Change libinput parameters, restart weston now to disable mouse or keyboard")

    def enable(self, time_sec = 2):
        ret = super().set_section(self.input_section, self.disable_libinput, False)
        if ret == Ret.PARAM_CHANGE:
            # Add delay because other processes need time to exit
            if time_sec:
                time.sleep(time_sec)
            super().restart(0)
            print ("INFO: Change libinput parameters, restart weston now to enable mouse or keyboard")

# Configure and enable pipewire backend
class PipewireBackend (WestonConfig):
    def is_exist (self):
        file_name = "pipewire-backend.so"
        for root, dirs, files in os.walk("/usr/lib/"):
            if file_name in files:
                return True
        return False

    def set_parameters(self, is_add):
        ret = Ret.OK
        section = ["[output]", "name=pipewire", "mirror-of=HDMI-A-1", "mode=1920x1080@60"]
        append_str=f"{section[0]}\n{section[1]}\n{section[2]}\n{section[3]}"
        has_pipewire = False
        remove_line = 0

        try:
            with open(self.file_name, "r+", encoding="utf-8") as file:
                lines = file.readlines()
                line_idx = 0
                file.seek(0)
                file.truncate()

                for line in lines:
                    # Check if the pipewire paramters are added
                    if (line_idx + 2) < len(lines):
                        output_pos = line.find(section[0])
                        next_line = lines[line_idx + 1]
                        name_pos = next_line.find(section[1])
                        # Pipewire parameters have been added already
                        if output_pos == 0 and name_pos == 0:
                            has_pipewire = True
                    elif (line_idx + 1) == len(lines):
                        # Append the paramter at the end of the file
                        if has_pipewire == False:
                            has_pipewire = True
                            line = line + append_str
                            print ("INFO: Add the parameters for pipewire output")
                            ret = Ret.PARAM_CHANGE

                    # Restore the pipewire parameters
                    if is_add == False and has_pipewire:
                        if remove_line < len(section):
                            line = ""
                            remove_line += 1
                        else:
                            remove_line = 0
                        #print (f"INFO: Remove line={remove_line}")
                        ret = Ret.PARAM_CHANGE

                    line_idx += 1
                    if remove_line == 0:
                        file.write (line)
                file.flush

        except FileNotFoundError:
            print ("Error: fail to configure the file: {self.file_name}")
            ret = Ret.ERROR_FILE_NO_FOUND

        return ret

    def set_backend(self, is_enable):
        return super().set_section("[core]", "backends=drm,pipewire\n", is_enable)

    def enable (self):
        ret = Ret.ERROR
        if self.is_exist() == False:
            print (f"ERROR: failed to find pipeiwre backend file, please install it first!")
            return ret
        # Set the parameters first and then enable the backend
        res_1 = self.set_parameters(True)
        if res_1 == Ret.OK or res_1 == Ret.PARAM_CHANGE:
            res_2 = self.set_backend(True)
            if res_2 == Ret.OK or res_2 == Ret.PARAM_CHANGE:
                if res_1 == Ret.PARAM_CHANGE or res_2 == Ret.PARAM_CHANGE:
                    super().restart(2)
                    print ("INFO: Change weston parameters, restart weston server now to enable pipewire backend")
                ret = Ret.OK
        if ret == Ret.ERROR:
            print (f"ERROR: failed to enable pipeiwre backend!")
        return ret

    def disable (self, time_sec = 2):
        ret = Ret.ERROR
        if self.is_exist() == False:
            print (f"ERROR: failed to find pipeiwre backend file, please install it first!")
            return ret
        # Disable the backend first and then remove the parameters
        res_1 = self.set_backend(False)
        if res_1 == Ret.OK or res_1 == Ret.PARAM_CHANGE:
            res_2 = self.set_parameters(False)
            if res_2 == Ret.OK or res_2 == Ret.PARAM_CHANGE:
                if res_1 == Ret.PARAM_CHANGE or res_2 == Ret.PARAM_CHANGE:
                    # Add delay because other processes need time to exit
                    if time_sec:
                        time.sleep(time_sec)
                    super().restart(0)
                    print ("INFO: Change weston parameters, restart weston server now to disable pipewire backend")
                ret = Ret.OK
        if ret == Ret.ERROR:
            print (f"ERROR: failed to disable pipeiwre backend!")
        return ret

# Check and start pipewire and the related servers if needed
class PipewireServer:
    def is_running(self):
        daemon_str = "/usr/bin/pipewire"
        str = os.popen(f"ps aux | grep {daemon_str}").read()
        daemon_line = str.split("\n")
        if daemon_line:
            id = daemon_line[0].split(" ")
            if (id[-1] == daemon_str):
                return True
        return False
    def start(self):
        os.system("systemctl --user --now enable pipewire wireplumber pipewire-pulse")
        #print ("INFO: Start pipewire server")

# Screen recorder base class
class ScreenRecorder:
    # audio node id to be used to read audio data which is created by pipewire
    audio_id = ""
    work_path = "/tmp/pipewire_recorder"
    record_process = ""

    def __init__ (self, device, has_audio, audio_card, user_pipeline, keepalive_time = 33):
        self.device = device
        self.has_audio = has_audio
        self.audio_card = audio_card
        self.user_pipeline = user_pipeline
        self.keepalive_time = keepalive_time

    def get_video_recorder_id(self):
        result = os.popen ('pw-top -b -n 1 | grep weston.pipewire | awk -F " " \'{print $2}\'').read()
        id = result.split("\n")
        return id[0]

    def get_audio_recorder_id(self):
        # Get WM series audio node id by default
        if self.audio_card == "":
            result = os.popen ('pw-top -b -n 1 | grep alsa_output.platform-sound-wm | awk -F " " \'{print $2}\'').read()
        else:
            # Get node id of the selected audio card
            cmd = (
                f"pw-top -b -n 1 | "
                f"grep alsa_output.platform-sound-{self.audio_card} | "
                f"awk '{{print $2}}'"
            )
            result = subprocess.check_output(cmd, shell=True, text=True).strip()

        audio_info = result.split("\n")
        if audio_info[0] == "":
            print ("ERROR: Failed to get audio recorder id!")
            return Ret.ERROR
        else:
            self.audio_id = audio_info[0]
            os.system (f"wpctl set-default {self.audio_id}")
            print (f"INFO: Set the default audio sink by wpctl, sink id: {self.audio_id}")
            return Ret.OK

    def get_local_ip(self):
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.connect(('8.8.8.8', 80))
            local_ip = s.getsockname()[0]
        finally:
            s.close()
        return local_ip

    def enable_device (self):
        # Try to configure backend and enable it
        ret = self.device.enable()
        # Get recorder id only if backend is valid
        if ret == Ret.OK:
            self.video_recorder_id = self.get_video_recorder_id()
            if self.video_recorder_id == "":
                ret = Ret.ERROR
                print ("ERROR: failed to get video recorder id!")

            if self.has_audio:
                ret = self.get_audio_recorder_id()
            if ret == Ret.OK:
                try:
                    if not os.path.isdir(self.work_path):
                        os.mkdir(self.work_path)
                        #print (f"create directory to store files: {self.work_path}")
                except OSError as e:
                    print(f"ERROR: {e}, failed to create directory: {self.work_path}")
                    ret = Ret.ERROR
        return ret

    def print_client_cmd(self, record_type, server_ip, payloader, audio_record, remote_control):
        cmd = "/usr/bin/remote_desktop.py --mode 0 "

        if record_type:
            cmd += f"--record_type {record_type} "

        if server_ip != "":
            cmd += f"--server_ip {server_ip} "

        if payloader:
            cmd += f"--payloader {payloader} "

        if audio_record:
            cmd += f"--audio_record {audio_record} "

        if remote_control == 0:
            cmd += f"--remote_control {remote_control} "

        print (f"INFO: Please run the command on the client side: {cmd}\n")

    def start (self, record_cmd):
        if record_cmd == "":
            print ("ERROR: Record command is empty!")
            return Ret.ERROR
        self.record_process = subprocess.Popen(record_cmd, shell=True)
        return Ret.OK

    def stop (self):
        if self.record_process != "":
            self.record_process.terminate()
        return Ret.OK

    def clear_file (self, path, file_name):
        all_files = os.listdir(path)
        for file in all_files:
            if fnmatch.fnmatch(file, file_name):
                path_file = os.path.join(path, file)
                os.remove (path_file)
                print (f"INFO: Remove recording file: {path_file}")

# Record screen data to the file
class FileRecorder (ScreenRecorder):
    def start (self):
        if self.user_pipeline != "":
            print (f"INFO: Run user-defined pipeline: {self.user_pipeline}")
            return super().start(self.user_pipeline)

        if super().enable_device() != Ret.OK:
            print ("ERROR: Failed to start recording to the file")
            return Ret.ERROR
        # Start recording and store it to the file
        current_time = datetime.datetime.now().strftime('%Y-%m-%d_%H-%M-%S')
        FILE_PATH = f"{self.work_path}/screen_record_{current_time}.mkv"
        if self.has_audio:
            record_cmd = (
                f"gst-launch-1.0 pipewiresrc path={self.video_recorder_id} keepalive-time={self.keepalive_time} provide-clock=false ! "
                f"imxvideoconvert_g2d ! v4l2h264enc extra-controls=\"encode, video_bitrate=10000000\" ! h264parse ! queue ! mux. "
                f"pipewiresrc path={self.audio_id} use-bufferpool=false provide-clock=false ! audio/x-raw,format=S16LE,rate=48000,channels=2 ! "
                f"queue ! lamemp3enc ! mpegaudioparse ! queue ! mux. matroskamux name=mux ! filesink location={FILE_PATH} >>/dev/null 2>&1"
            )
        else:
            record_cmd = (
                f"gst-launch-1.0 pipewiresrc path={self.video_recorder_id} keepalive-time={self.keepalive_time} provide-clock=false ! "
                f"imxvideoconvert_g2d ! v4l2h264enc extra-controls=\"encode, video_bitrate=10000000\" ! h264parse ! queue ! matroskamux ! "
                f"filesink location={FILE_PATH} >>/dev/null 2>&1"
            )
        super().start(record_cmd)
        print (f"INFO: Start recording to the file, location: {FILE_PATH}")
        return Ret.OK

    def stop (self):
        super().stop()
        print ("Stop file recording to the file")

# Record screen data to HLS stream
class HlsRecorder (ScreenRecorder):
    hls_playlist = "screen_play.m3u8"
    hls_segment = "screen_segment"
    http_process = ""
    port = 9999

    def start (self):
        if self.user_pipeline != "":
            print (f"INFO: Run user-defined pipeline: {self.user_pipeline}")
            return super().start(self.user_pipeline)

        if super().enable_device() != Ret.OK:
            print ("ERROR: failed to start recording as HLS server")
            return Ret.ERROR

        # Clear out the remaining files if they exist
        self.clear_file (self.work_path, self.hls_playlist)
        self.clear_file (self.work_path, f"{self.hls_segment}*.ts")

        # Start http server
        http_server_cmd = f"nohup python3 -m http.server {self.port} --directory {self.work_path} >>/dev/null 2>&1"
        self.http_process = subprocess.Popen(http_server_cmd, shell=True)

        # Start recording
        server_ip = self.get_local_ip()
        if self.has_audio:
            record_cmd = (
                f"gst-launch-1.0 pipewiresrc path={self.video_recorder_id} keepalive-time={self.keepalive_time} provide-clock=false ! "
                f"imxvideoconvert_g2d ! v4l2h264enc extra-controls=\"encode, video_bitrate=10000000\" ! h264parse ! queue ! mux. "
                f"pipewiresrc path={self.audio_id} use-bufferpool=false provide-clock=false ! audio/x-raw,format=S16LE,rate=48000,channels=2 ! "
                f"queue ! lamemp3enc ! mpegaudioparse ! queue ! mux. mpegtsmux name=mux ! hlssink playlist-root=http://{server_ip}:{self.port} "
                f"playlist-location={self.work_path}/{self.hls_playlist} location={self.work_path}/{self.hls_segment}_%05d.ts target-duration=1 max-files=5 >>/dev/null 2>&1"
            )
        else:
            record_cmd = (
                f"gst-launch-1.0 pipewiresrc path={self.video_recorder_id} keepalive-time={self.keepalive_time} provide-clock=false ! "
                f"imxvideoconvert_g2d ! v4l2h264enc extra-controls=\"encode, video_bitrate=10000000\" ! h264parse ! queue ! mpegtsmux ! "
                f"hlssink playlist-root=http://{server_ip}:{self.port} playlist-location={self.work_path}/{self.hls_playlist} "
                f"location={self.work_path}/{self.hls_segment}_%05d.ts target-duration=1 max-files=5 >>/dev/null 2>&1"
            )
        super().start(record_cmd)
        print (f"INFO: Start recording as HLS server. Play URI: http://{server_ip}:{self.port}/{self.hls_playlist}")
        print (f"Recording file directory: {self.work_path}")
        return Ret.OK

    def stop (self):
        super().stop()
        if self.http_process != "":
            self.http_process.terminate()
        print ("INFO: Stop recording as a HLS server")

        # Clear out the remaining files
        time.sleep(1)
        self.clear_file (self.work_path, self.hls_playlist)
        self.clear_file (self.work_path, f"{self.hls_segment}*.ts")

# Record screen data to RTSP stream
class RtspRecorder (ScreenRecorder):
    rtsp_server_path = "/usr/bin"
    rtsp_server = "test-launch"

    def __init__ (self, device, has_audio, payloader, audio_card, user_pipeline, keepalive_time = 33, remote_control = 1):
        super().__init__(device, has_audio, audio_card, user_pipeline, keepalive_time)
        self.remote_control = remote_control
        self.payloader = payloader
        if self.has_audio:
            self.payloader = StreamPayloader.RTP_MP2_PAY
            print ("INFO: Palyloader is fixed to rtpmp2tpay for AV stream")

    def rtsp_server_is_exist (self, path, server_name):
        all_files = os.listdir(path)
        for file in all_files:
            if fnmatch.fnmatch(file, server_name):
                return True
        return False
    
    def start (self):
        if self.user_pipeline != "":
            print (f"INFO: Run user-defined pipeline: {self.user_pipeline}")
            return super().start(self.user_pipeline)

        if super().enable_device() != Ret.OK:
            print ("ERROR: Failed to start recording as RTSP server")
            return Ret.ERROR

        # Install server if needed
        if self.rtsp_server_is_exist(self.rtsp_server_path, self.rtsp_server) == False:
            print (f"ERROR: {self.rtsp_server} is not found. Please install it to {self.rtsp_server_path}/")
            return Ret.ERROR
        # Configure payloader type

        if self.has_audio:
            record_cmd = (
                f"{self.rtsp_server_path}/{self.rtsp_server} \"pipewiresrc path={self.video_recorder_id} keepalive-time={self.keepalive_time} provide-clock=false ! "
                f"imxvideoconvert_g2d ! v4l2h264enc extra-controls=encode,video_bitrate=10000000 ! h264parse ! queue ! mux. "
                f"pipewiresrc path={self.audio_id} use-bufferpool=false provide-clock=false ! audio/x-raw,format=S16LE,rate=48000,channels=2 ! queue ! lamemp3enc ! "
                f"mpegaudioparse ! queue ! mux. mpegtsmux name=mux ! rtpmp2tpay name=pay0 >>/dev/null 2>&1\""
            )
        else:
            if self.payloader == StreamPayloader.RTP_H264_PAY:
                rtp_pay = "rtph264pay name=pay0"
            elif self.payloader == StreamPayloader.RTP_MP2_PAY:
                rtp_pay = "mpegtsmux ! rtpmp2tpay name=pay0"
            else:
                print ("ERROR: Unsupported encoder type")
                return Ret.ERROR
            record_cmd = (
                f"{self.rtsp_server_path}/{self.rtsp_server} \"pipewiresrc path={self.video_recorder_id} keepalive-time={self.keepalive_time} provide-clock=false ! "
                f"imxvideoconvert_g2d ! v4l2h264enc extra-controls=encode,video_bitrate=10000000 ! h264parse ! queue ! {rtp_pay} >>/dev/null 2>&1\""
            )
        # Start recording
        super().start (record_cmd)

        server_ip = self.get_local_ip()
        print (f"INFO: Start recording as RTSP server. Play URI: rtsp://{server_ip}:8554/test")
        self.print_client_cmd(0, server_ip, self.payloader, self.has_audio, self.remote_control)
        return Ret.OK

    def stop (self):
        super().stop()
        print ("INFO: Stop RTSP server")

# Record screen data to RTP stream
class RtpRecorder (ScreenRecorder):
    def __init__ (self, device, has_audio, payloader, receiver_ip, audio_card, user_pipeline, keepalive_time = 33, remote_control = 1):
        super().__init__(device, has_audio, audio_card, user_pipeline, keepalive_time)
        self.receiver_ip = receiver_ip
        self.payloader = payloader
        self.remote_control = remote_control
        if self.has_audio:
            self.payloader = StreamPayloader.RTP_MP2_PAY
            print ("INFO: Palyloader is fixed to rtpmp2tpay for AV stream")

    def start (self):
        if self.user_pipeline != "":
            print (f"INFO: Run user-defined pipeline: {self.user_pipeline}")
            return super().start(self.user_pipeline)

        if self.receiver_ip == "":
            print ("ERROR: Receiver IP is not set")
            return Ret.ERROR

        if super().enable_device() != Ret.OK:
            print ("ERROR: Failed to start recording as RTP server")
            return Ret.ERROR

        # Configure cmd
        if self.has_audio:
            record_cmd = (
                f"gst-launch-1.0 pipewiresrc path={self.video_recorder_id} keepalive-time={self.keepalive_time} provide-clock=false ! imxvideoconvert_g2d ! "
                f"v4l2h264enc extra-controls=\"encode, video_bitrate=10000000\" ! h264parse ! queue ! mux. "
                f"pipewiresrc path={self.audio_id} use-bufferpool=false provide-clock=false ! audio/x-raw,format=S16LE,rate=48000,channels=2 ! queue ! "
                f"lamemp3enc ! mpegaudioparse ! queue ! mux. mpegtsmux name=mux ! rtpmp2tpay ! udpsink host={self.receiver_ip} port=1234 >>/dev/null 2>&1"
            )
        else:
            if self.payloader == StreamPayloader.RTP_H264_PAY:
                rtp_pay = "rtph264pay"
            elif self.payloader == StreamPayloader.RTP_MP2_PAY:
                rtp_pay = "mpegtsmux ! rtpmp2tpay"
            else:
                print ("ERROR: Unsupported encoder type")
                return Ret.ERROR
            record_cmd = (
                f"gst-launch-1.0 pipewiresrc path={self.video_recorder_id} keepalive-time={self.keepalive_time} provide-clock=false ! imxvideoconvert_g2d ! "
                f"v4l2h264enc extra-controls=\"encode, video_bitrate=10000000\" ! h264parse ! queue ! {rtp_pay} ! "
                f"udpsink host={self.receiver_ip} port=1234 sync=false async=false >>/dev/null 2>&1"
            )

        # Start recording
        super().start(record_cmd)

        server_ip = self.get_local_ip()
        print (f"INFO: Start recording as RTP server. Play URI: rtp://@{self.receiver_ip}:1234")

        self.print_client_cmd(1, server_ip, self.payloader, self.has_audio, self.remote_control)
        return Ret.OK

    def stop (self):
        super().stop()
        print ("INFO: Stop RTP server")

# Remote control class
class RemoteControl:
    input_path = "/usr/bin"
    input_server = "remote_input"
    input_process = ""

    def remote_input_is_exist (self, path, server_name):
        all_files = os.listdir(path)
        for file in all_files:
            if fnmatch.fnmatch(file, server_name):
                return True
        return False
    
    def get_local_ip(self):
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.connect(('8.8.8.8', 80))
            local_ip = s.getsockname()[0]
        finally:
            s.close()
        return local_ip

    def start (self, mode, server_ip):
        # Check remote input is exist
        if self.remote_input_is_exist(self.input_path, self.input_server) == False:
            print (f"ERROR: {self.input_server} is not found. Please install it to {self.input_path}/")
            return Ret.ERROR
        # Start remote control 
        if mode == WorkMode.SERVER:
            server_ip = self.get_local_ip()

        control_cmd = f"{self.input_path}/{self.input_server} -r {mode} -o {server_ip}"
        self.input_process = subprocess.Popen(control_cmd, shell=True)
        print ("INFO: Start remote control")
        return Ret.OK

    def stop (self):
        if self.input_process != "":
            self.input_process.terminate()
        print ("INFO: Stop remote control")

# Screen Player base class
class ScreenPlayer:
    play_process = ""
    audio_device = ""

    def __init__ (self, has_audio, payloader, audio_card = "", user_pipeline = ""):
        self.has_audio = has_audio
        self.payloader = payloader
        self.audio_card = audio_card
        self.user_pipeline = user_pipeline
        self.log_redirect = " >>/dev/null 2>&1"

    def get_audio_device (self):
        if self.audio_card == "":
            # Get the index of WM series audio card
            result = os.popen ("aplay -l | grep wm | awk -F \"[: ]\" '{print $2}'").read()
        else:
            # Get the index of the selected audio card
            cmd = f"aplay -l | grep {self.audio_card} | awk -F '[: ]' '{{print $2}}'"
            result = subprocess.check_output(cmd, shell=True, text=True).strip()
        audio_index = result.split("\n")
        if audio_index[0] == "":
            print (f"ERROR: Can not get audio card index")
            return Ret.ERROR
        else:
            print (f"INFO: Select audio card index: {audio_index[0]}")
            self.audio_device = f"plughw:{audio_index[0]},0"
            return Ret.OK

    def start (self, play_cmd):
        if play_cmd == "":
            print ("ERROR: play command is empty!")
            return Ret.ERROR
        self.play_process = subprocess.Popen(play_cmd + self.log_redirect, shell=True)
        return Ret.OK

    def stop (self):
        if self.play_process != "":
            self.play_process.terminate()
        print ("INFO: Stop playing")

# RTSP player for client mode
class RtspPlayer (ScreenPlayer):
    def __init__ (self, has_audio, payloader, ip, audio_card = "", user_pipeline = ""):
        super().__init__ (has_audio, payloader, audio_card, user_pipeline)
        self.ip = ip

    def start (self):
        if self.user_pipeline != "":
            print (f"INFO: Run user-defined pipeline: {self.user_pipeline}")
            return super().start(self.user_pipeline)

        if self.has_audio:
            ret = super().get_audio_device()
            if ret == Ret.ERROR:
                return ret
            play_cmd = (
                f"gst-launch-1.0 rtspsrc location=\"rtsp://{self.ip}:8554/test\" latency=0 ! rtpmp2tdepay ! typefind ! aiurdemux streaming_latency=0 name=d d. ! "
                f"queue ! h264parse ! v4l2h264dec extra-controls=\"decode, display_delay_enable=1\" ! queue max-size-bytes=0 ! "
                f"waylandsink show-preroll-frame=false processing-deadline=0 d. ! queue ! audio/mpeg,framed=true ! beepdec ! queue ! "
                f"alsasink device={self.audio_device} buffer-time=40000 processing-deadline=180000000"
            )
        else:
            if self.payloader == StreamPayloader.RTP_H264_PAY:
                play_cmd = (
                    f"gst-launch-1.0 rtspsrc location=\"rtsp://{self.ip}:8554/test\" latency=0 ! rtph264depay ! h264parse ! "
                    f"v4l2h264dec extra-controls=\"decode, display_delay_enable=1\" ! queue max-size-bytes=0 ! "
                    f"waylandsink show-preroll-frame=false processing-deadline=0"
                )
            elif self.payloader == StreamPayloader.RTP_MP2_PAY:
                play_cmd = (
                    f"gst-launch-1.0 rtspsrc location=\"rtsp://{self.ip}:8554/test\" latency=0 ! rtpmp2tdepay ! typefind ! aiurdemux streaming_latency=30 ! "
                    f"queue ! h264parse ! v4l2h264dec extra-controls=\"decode, display_delay_enable=1\" ! queue max-size-bytes=0 ! "
                    f"waylandsink show-preroll-frame=false processing-deadline=0"
                )
            else:
                print ("ERROR: Unsupported payloader type")
                return Ret.ERROR

        super().start(play_cmd)
        print (f"INFO: Playing RTSP stream by URI: rtsp://{self.ip}:8554/test")
        return Ret.OK

    def stop (self):
        super().stop()

# RTP player for client mode
class RtpPlayer (ScreenPlayer):
    def start (self):
        if self.user_pipeline != "":
            print (f"INFO: Run user-defined pipeline: {self.user_pipeline}")
            return super().start(self.user_pipeline)

        if self.has_audio:
            ret = super().get_audio_device()
            if ret == Ret.ERROR:
                return ret
            play_cmd = (
                f"gst-launch-1.0 udpsrc port=1234 caps=\"application/x-rtp\" ! rtpmp2tdepay ! typefind ! aiurdemux streaming_latency=30 name=d d. ! queue ! "
                f"h264parse ! v4l2h264dec extra-controls=\"decode, display_delay_enable=1\" ! queue max-size-bytes=0 ! "
                f"waylandsink show-preroll-frame=false processing-deadline=0 d. ! queue ! audio/mpeg,framed=true ! beepdec ! queue ! "
                f"alsasink device={self.audio_device} buffer-time=40000 processing-deadline=180000000"
            )
        else:
            if self.payloader == StreamPayloader.RTP_H264_PAY:
                play_cmd = (
                    f"gst-launch-1.0 udpsrc port=1234 caps=\"application/x-rtp\" ! rtph264depay ! h264parse ! "
                    f"v4l2h264dec extra-controls=\"decode, display_delay_enable=1\" ! queue max-size-bytes=0 ! "
                    f"waylandsink show-preroll-frame=false processing-deadline=0"
                )
            elif self.payloader == StreamPayloader.RTP_MP2_PAY:
                play_cmd = (
                    f"gst-launch-1.0 udpsrc port=1234 caps=\"application/x-rtp\" ! rtpmp2tdepay ! typefind ! aiurdemux streaming_latency=30 ! queue ! "
                    f"h264parse ! v4l2h264dec extra-controls=\"decode, display_delay_enable=1\" ! queue max-size-bytes=0 ! "
                    f"waylandsink show-preroll-frame=false processing-deadline=0"
                )
            else:
                print ("ERROR: Unsupported encoder type")
                return Ret.ERROR

        super().start(play_cmd)
        print (f"INFO: Playing RTP stream")
        return Ret.OK

    def stop (self):
        super().stop()

if __name__ == "__main__":
    arg_parser = argparse.ArgumentParser(description='Start screen recording. Record as HLS live server if the record type is not selected')
    arg_parser.add_argument('--mode', type=int, default=1, help='Work mode. It can be 1(server mode by default) or 0(client mode)')
    arg_parser.add_argument('--record_type', type=int, default=0, help='Stream record type. It can be 0(RTSP by default), 1(RTP), 2(HLS), 3(record to file)')
    arg_parser.add_argument('--rtp_ip', type=str, default="", help='Receiver ip address only for RTP record type. Only used in server mode.')
    arg_parser.add_argument('--audio_record', type=int, default=0, help='Record audio simultaneously. It can be 1(enable) or 0(disable by default)')
    arg_parser.add_argument('--restore_params', type=int, default=0, help='Restore pipewire backend parameters when the script exists. It can be is 1(enable) or 0(disable by default)')
    arg_parser.add_argument('--remote_control', type=int, default=1, help='Enable remote control. It can be 1(enable by default) or 0(disable)')
    arg_parser.add_argument('--server_ip', type=str, default="", help='Remote server ip address for remote control and RTSP stream. Only used in client mode.')
    arg_parser.add_argument('--payloader', type=int, default=0, help='Stream payloader type. It can be 0(rtpmp2tpay by default) or 1(rtph264pay).')
    arg_parser.add_argument('--pipeline', type=str, default="", help='Run user-defined pipeline instead of built-in pipeline(run build-in pipeline by default).')
    arg_parser.add_argument('--audio_card', type=str, default="wm", help='Select audio card name for audio recording(Select WM series card by default).')
    arg_parser.add_argument('--keepalive_time', type=int, default="33", help='Periodically send last buffer in milliseconds if weston did not repaint screen(33ms by default).')

    print (VERSION_STR)
    args = arg_parser.parse_args()
    ret = Ret.OK
    player = None
    recorder = None

    if args.mode == WorkMode.CLIENT:
        print ("INFO: Work in client mode")
        # Check and disable libinput for remote control
        if args.remote_control:
            libinput = Libinput()
            libinput.disable()

        # Create player instance based on record type
        if args.record_type == RecordType.RTSP:
            if args.server_ip == "":
                print ("ERROR: Invalid server ip address, please check it")
                ret = Ret.ERROR
            else:
                player = RtspPlayer(args.audio_record, args.payloader, args.server_ip, args.audio_card, args.pipeline)
                ret = player.start()
        elif args.record_type == RecordType.RTP:
            if args.remote_control and args.server_ip == "":
                print ("ERROR: Invalid server ip address, please check it")
                ret = Ret.ERROR
            else:
                player = RtpPlayer(args.audio_record, args.payloader, args.audio_card, args.pipeline)
                ret = player.start()
        else:
            print ("ERROR: Unsupported stream type")
            ret = Ret.ERROR
    elif args.mode == WorkMode.SERVER:
        print ("INFO: Work in server mode")
        # Check and enable pipewire service if needed
        pipewire_server = PipewireServer()
        if pipewire_server.is_running() == False:
            pipewire_server.start()
            print ("INFO: Start pipewire")

        # Enable pipewire backend
        pipewire_backend = PipewireBackend()

        # Create recorder instance based on record type
        if args.record_type == RecordType.RTSP:
            recorder = RtspRecorder(pipewire_backend, args.audio_record, args.payloader, args.audio_card, args.pipeline, args.keepalive_time, args.remote_control)
        elif args.record_type == RecordType.RTP:
            recorder = RtpRecorder(pipewire_backend, args.audio_record, args.payloader, args.rtp_ip, args.audio_card, args.pipeline, args.keepalive_time, args.remote_control)
        elif args.record_type == RecordType.HLS:
            recorder = HlsRecorder(pipewire_backend, args.audio_record, args.audio_card, args.pipeline, args.keepalive_time)
        elif args.record_type == RecordType.FILE:
            recorder = FileRecorder(pipewire_backend, args.audio_record, args.audio_card, args.pipeline, args.keepalive_time)
        else:
            print ("ERROR: Unknown record type, please check the argument")
            sys.exit(0)
        ret = recorder.start()
    else:
        print ("ERROR: Unknown record mode, please check the argument")
        sys.exit(0)

    # Create a remote control instance if enabled
    if args.remote_control:
        remote_control = RemoteControl()
        if remote_control.start(args.mode, args.server_ip) != Ret.OK:
            print ("ERROR: Failed to enable remote control, please check the configuration")

    try:
        if ret == Ret.OK:
            while True:
                time.sleep (1)
    except KeyboardInterrupt:
        pass
    finally:
        if args.remote_control:
            remote_control.stop()

        if args.mode == WorkMode.CLIENT:
            if player:
                player.stop()

            # Check and enable libinput again if it's disabled
            if args.remote_control:
                libinput.enable()
        else:
            if recorder:
                recorder.stop()

            if args.restore_params:
                pipewire_backend.disable()

    print("INFO: Exit remote desktop program\n")
    sys.exit(0)

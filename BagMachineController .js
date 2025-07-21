import React, { useEffect, useState, useRef } from 'react';
import {
  View,
  Text,
  StyleSheet,
  useWindowDimensions,
  TouchableOpacity,
  Animated,
  ActivityIndicator,
} from 'react-native';

let toastTimer;

const BagMachineController = () => {
  const { width, height } = useWindowDimensions();
  const isLandscape = width > height;

  const [bagLengthCM, setBagLengthCM] = useState('');
  const [bpm, setBpm] = useState('');
  const [machineStatus, setMachineStatus] = useState('stopped');
  const [toastMsg, setToastMsg] = useState('');

  const [loadingBagLength, setLoadingBagLength] = useState(false);
  const [loadingBPM, setLoadingBPM] = useState(false);
  const [loadingStart, setLoadingStart] = useState(false);
  const [loadingStop, setLoadingStop] = useState(false);
  const [loadingRefresh, setLoadingRefresh] = useState(false);

  const blinkAnim = useRef(new Animated.Value(1)).current;
  const toastOpacity = useRef(new Animated.Value(0)).current;

  useEffect(() => {
    Animated.loop(
      Animated.sequence([
        Animated.timing(blinkAnim, { toValue: 0.3, duration: 600, useNativeDriver: true }),
        Animated.timing(blinkAnim, { toValue: 1, duration: 600, useNativeDriver: true }),
      ])
    ).start();
  }, []);

  useEffect(() => {
    fetchInitialData();
    return () => clearTimeout(toastTimer);
  }, []);

  const fetchInitialData = async () => {
    setLoadingRefresh(true);
    try {
      const statusRes = await fetch(`http://192.168.4.1/getmachinestate`);
      const statusText = await statusRes.text();
      setMachineStatus(statusText.trim());

      const lengthRes = await fetch(`http://192.168.4.1/getbaglength`);
      const lengthMM = parseInt(await lengthRes.text());
      setBagLengthCM(lengthMM.toString());

      const bpmRes = await fetch(`http://192.168.4.1/getbpm`);
      const bpmText = await bpmRes.text();
      setBpm(parseInt(bpmText.trim()) || 30);
    } catch (err) {
      showToast('⚠️ ESP32 not reachable');
    }
    setLoadingRefresh(false);
  };

  const showToast = (msg) => {
    setToastMsg(msg);
    Animated.timing(toastOpacity, {
      toValue: 1,
      duration: 200,
      useNativeDriver: true,
    }).start();

    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => {
      Animated.timing(toastOpacity, {
        toValue: 0,
        duration: 300,
        useNativeDriver: true,
      }).start(() => setToastMsg(''));
    }, 4000);
  };

  const saveBagLength = async () => {
    const cm = parseInt(bagLengthCM);
    setLoadingBagLength(true);
    try {
      const res = await fetch(`http://192.168.4.1/baglength?data=${cm}`);
      res.ok ? showToast(`Bag length updated: ${cm} cm`) : showToast('Save failed');
    } catch {
      showToast('ESP32 not reachable');
    }
    setLoadingBagLength(false);
  };

  const saveBPM = async () => {
    setLoadingBPM(true);
    try {
      const res = await fetch(`http://192.168.4.1/bpm?data=${bpm}`);
      res.ok ? showToast(`BPM set to ${bpm}`) : showToast('Failed to set BPM');
    } catch {
      showToast('ESP32 not reachable');
    }
    setLoadingBPM(false);
  };

  const adjustBagLength = (delta) => {
    const current = parseInt(bagLengthCM || '0');
    const next = Math.max(15, Math.min(40, current + delta));
    setBagLengthCM(next.toString());
  };

  const adjustBPM = (delta) => {
    const next = Math.max(20, Math.min(72, bpm + delta));
    setBpm(next);
  };

  const handleRun = async () => {
    setLoadingStart(true);
    try {
      const res = await fetch(`http://192.168.4.1/machinestart`);
      if (res.ok) {
        setMachineStatus('running');
        showToast('🟢 Machine Started');
      } else {
        showToast('Start failed');
      }
    } catch {
      showToast('ESP32 not reachable');
    }
    setLoadingStart(false);
  };

  const handleStop = async () => {
    setLoadingStop(true);
    try {
      const res = await fetch(`http://192.168.4.1/machinestop`);
      if (res.ok) {
        setMachineStatus('stopped');
        showToast('🔴 Machine Stopped');
      } else {
        showToast('Stop failed');
      }
    } catch {
      showToast('ESP32 not reachable');
    }
    setLoadingStop(false);
  };

  return (
    <View style={[styles.container, isLandscape && styles.landscape]}>
      <Animated.View
        style={[
          styles.statusBanner,
          {
            backgroundColor: machineStatus === 'running' ? '#00FFB3' : '#FF2D55',
            opacity: blinkAnim,
          },
        ]}
      >
        <Text style={styles.statusBannerText}>
          {machineStatus === 'running' ? '🟢 MACHINE IS RUNNING' : '🔴 MACHINE IS STOPPED'}
        </Text>
      </Animated.View>

      <View style={styles.section}>
        <Text style={styles.label}>📏 Bag Length (cm)</Text>
        <Text style={styles.counter}>{bagLengthCM}</Text>
        <View style={styles.row}>
          <TouchableOpacity onPress={() => adjustBagLength(-1)} style={styles.adjustButton}>
            <Text style={styles.adjustButtonText}>−</Text>
          </TouchableOpacity>
          <TouchableOpacity onPress={() => adjustBagLength(1)} style={styles.adjustButton}>
            <Text style={styles.adjustButtonText}>+</Text>
          </TouchableOpacity>
        </View>
        <TouchableOpacity onPress={saveBagLength} style={styles.saveButton} disabled={loadingBagLength}>
          {loadingBagLength ? (
            <ActivityIndicator size="small" color="#FFF" />
          ) : (
            <Text style={styles.saveButtonText}>💾 Set Bag Length</Text>
          )}
        </TouchableOpacity>
      </View>

      <View style={styles.section}>
        <Text style={styles.label}>⚙️ Speed (Bags/Min)</Text>
        <Text style={styles.counter}>{bpm}</Text>
        <View style={styles.row}>
          <TouchableOpacity onPress={() => adjustBPM(-1)} style={styles.adjustButton}>
            <Text style={styles.adjustButtonText}>−</Text>
          </TouchableOpacity>
          <TouchableOpacity onPress={() => adjustBPM(1)} style={styles.adjustButton}>
            <Text style={styles.adjustButtonText}>+</Text>
          </TouchableOpacity>
        </View>
        <TouchableOpacity
          onPress={saveBPM}
          style={[styles.saveButton, { backgroundColor: '#0EA5E9' }]}
          disabled={loadingBPM}
        >
          {loadingBPM ? (
            <ActivityIndicator size="small" color="#FFF" />
          ) : (
            <Text style={styles.saveButtonText}>💾 Set BPM</Text>
          )}
        </TouchableOpacity>
      </View>

      <View style={styles.section}>
        <TouchableOpacity style={styles.runButton} onPress={handleRun} disabled={loadingStart}>
          {loadingStart ? (
            <ActivityIndicator size="small" color="#FFF" />
          ) : (
            <Text style={styles.runButtonText}>▶️ Start</Text>
          )}
        </TouchableOpacity>
        <TouchableOpacity style={styles.stopButton} onPress={handleStop} disabled={loadingStop}>
          {loadingStop ? (
            <ActivityIndicator size="small" color="#FFF" />
          ) : (
            <Text style={styles.stopButtonText}>⛔ Stop</Text>
          )}
        </TouchableOpacity>
      </View>

      <TouchableOpacity
        style={styles.refreshButton}
        onPress={fetchInitialData}
        disabled={loadingRefresh}
      >
        {loadingRefresh ? (
          <ActivityIndicator size="small" color="#FFF" />
        ) : (
          <Text style={styles.refreshButtonText}>🔁 Refresh Data</Text>
        )}
      </TouchableOpacity>

      {toastMsg !== '' && (
        <Animated.View style={[styles.toast, { opacity: toastOpacity }]}>
          <Text style={styles.toastText}>{toastMsg}</Text>
        </Animated.View>
      )}
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#0F172A',
    paddingTop: 10,
    alignItems: 'center',
    justifyContent: 'space-evenly',
  },
  landscape: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    justifyContent: 'space-evenly',
  },
  statusBanner: {
    width: '90%',
    paddingVertical: 10,
    borderRadius: 14,
    marginBottom: 10,
    alignItems: 'center',
  },
  statusBannerText: {
    color: '#F8FAFC',
    fontSize: 18,
    fontWeight: 'bold',
  },
  section: {
    backgroundColor: '#1E293B',
    padding: 12,
    margin: 6,
    borderRadius: 16,
    elevation: 3,
    alignItems: 'center',
    width: 280,
  },
  label: {
    fontSize: 16,
    fontWeight: '600',
    marginBottom: 6,
    color: '#A5F3FC',
  },
  counter: {
    fontSize: 28,
    fontWeight: 'bold',
    color: '#38BDF8',
    marginVertical: 6,
  },
  row: {
    flexDirection: 'row',
    marginVertical: 6,
  },
  adjustButton: {
    backgroundColor: '#334155',
    paddingVertical: 6,
    paddingHorizontal: 14,
    marginHorizontal: 10,
    borderRadius: 30,
  },
  adjustButtonText: {
    fontSize: 30,
    color: '#E0F2FE',
    fontWeight: 'bold',
  },
  saveButton: {
    backgroundColor: '#10B981',
    paddingVertical: 6,
    paddingHorizontal: 20,
    borderRadius: 10,
    marginTop: 6,
  },
  saveButtonText: {
    color: '#FFF',
    fontSize: 14,
    fontWeight: '600',
  },
  runButton: {
    backgroundColor: '#3B82F6',
    paddingVertical: 8,
    paddingHorizontal: 40,
    borderRadius: 10,
    marginTop: 8,
  },
  runButtonText: {
    color: '#FFF',
    fontSize: 16,
    fontWeight: '600',
  },
  stopButton: {
    backgroundColor: '#DC2626',
    paddingVertical: 8,
    paddingHorizontal: 40,
    borderRadius: 10,
    marginTop: 8,
  },
  stopButtonText: {
    color: '#FFF',
    fontSize: 16,
    fontWeight: '600',
  },
  refreshButton: {
    backgroundColor: '#64748B',
    paddingVertical: 8,
    paddingHorizontal: 24,
    borderRadius: 10,
    marginBottom: 6,
  },
  refreshButtonText: {
    color: '#FFF',
    fontSize: 14,
    fontWeight: '600',
  },
  toast: {
    position: 'absolute',
    top: 10,
    alignSelf: 'center',
    backgroundColor: '#11121e',
    paddingHorizontal: 20,
    paddingVertical: 10,
    borderRadius: 12,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 4 },
    shadowOpacity: 0.3,
    shadowRadius: 6,
    elevation: 10,
    zIndex: 999,
  },
  toastText: {
    color: '#E0F2FE',
    fontSize: 16,
    fontWeight: '600',
  },
});

export default BagMachineController;

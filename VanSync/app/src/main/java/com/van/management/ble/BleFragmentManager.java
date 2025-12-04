package com.van.management.ble;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Gestionnaire de fragmentation pour les commandes BLE trop grandes
 * 
 * Format du protocole de fragmentation:
 * 
 * Paquet de données (si taille <= MTU):
 * [type=0x00] [data...]
 * 
 * Premier fragment:
 * [type=0x01] [fragment_id(2)] [total_fragments(2)] [total_size(4)] [data...]
 * 
 * Fragments suivants:
 * [type=0x02] [fragment_id(2)] [fragment_index(2)] [data...]
 * 
 * Dernier fragment:
 * [type=0x03] [fragment_id(2)] [fragment_index(2)] [data...]
 */
public class BleFragmentManager {
    
    // Types de paquets
    private static final byte PACKET_TYPE_COMPLETE = 0x00;
    private static final byte PACKET_TYPE_FIRST_FRAGMENT = 0x01;
    private static final byte PACKET_TYPE_MIDDLE_FRAGMENT = 0x02;
    private static final byte PACKET_TYPE_LAST_FRAGMENT = 0x03;
    
    // Tailles des en-têtes
    private static final int COMPLETE_HEADER_SIZE = 1;
    private static final int FIRST_FRAGMENT_HEADER_SIZE = 9; // type(1) + id(2) + total(2) + size(4)
    private static final int FRAGMENT_HEADER_SIZE = 5; // type(1) + id(2) + index(2)
    
    private final int effectiveMtu;
    private int nextFragmentId = 0;
    
    /**
     * @param mtu MTU BLE (généralement 512)
     */
    public BleFragmentManager(int mtu) {
        this.effectiveMtu = mtu - 3; // Soustraire l'overhead BLE
    }
    
    /**
     * Fragmente les données si nécessaire
     * @param data Données complètes à envoyer
     * @return Liste des paquets à envoyer
     */
    public byte[][] fragmentData(byte[] data) {
        int maxDataPerPacket = effectiveMtu - COMPLETE_HEADER_SIZE;
        
        // Si les données tiennent en un seul paquet
        if (data.length <= maxDataPerPacket) {
            byte[][] result = new byte[1][];
            ByteBuffer buffer = ByteBuffer.allocate(data.length + COMPLETE_HEADER_SIZE);
            buffer.order(ByteOrder.LITTLE_ENDIAN);
            
            buffer.put(PACKET_TYPE_COMPLETE);
            buffer.put(data);
            
            result[0] = buffer.array();
            android.util.Log.d("BleFragmentMgr", "Paquet unique: " + result[0].length + " bytes");
            return result;
        }
        
        // Fragmentation nécessaire
        int fragmentId = getNextFragmentId();
        
        // Calculer le nombre de fragments
        int firstFragmentDataSize = effectiveMtu - FIRST_FRAGMENT_HEADER_SIZE;
        int middleFragmentDataSize = effectiveMtu - FRAGMENT_HEADER_SIZE;
        
        int remainingAfterFirst = data.length - firstFragmentDataSize;
        int middleFragmentCount = (int) Math.ceil((double) remainingAfterFirst / middleFragmentDataSize);
        int totalFragments = 1 + middleFragmentCount;
        
        byte[][] fragments = new byte[totalFragments][];
        int dataOffset = 0;
        
        // Premier fragment
        int firstDataSize = Math.min(firstFragmentDataSize, data.length);
        ByteBuffer firstBuffer = ByteBuffer.allocate(FIRST_FRAGMENT_HEADER_SIZE + firstDataSize);
        firstBuffer.order(ByteOrder.LITTLE_ENDIAN);
        firstBuffer.put(PACKET_TYPE_FIRST_FRAGMENT);
        firstBuffer.putShort((short) fragmentId);
        firstBuffer.putShort((short) totalFragments);
        firstBuffer.putInt(data.length);
        firstBuffer.put(data, dataOffset, firstDataSize);
        dataOffset += firstDataSize;
        
        fragments[0] = firstBuffer.array();
        
        android.util.Log.d("BleFragmentMgr", String.format("Fragment 0 (first): header=%d + data=%d = %d bytes (total_size=%d, total_fragments=%d)",
            FIRST_FRAGMENT_HEADER_SIZE, firstDataSize, fragments[0].length, data.length, totalFragments));
        
        // Fragments suivants
        for (int i = 1; i < totalFragments; i++) {
            boolean isLast = (i == totalFragments - 1);
            int fragmentDataSize = Math.min(middleFragmentDataSize, data.length - dataOffset);
            
            ByteBuffer fragmentBuffer = ByteBuffer.allocate(FRAGMENT_HEADER_SIZE + fragmentDataSize);
            fragmentBuffer.order(ByteOrder.LITTLE_ENDIAN);
            
            fragmentBuffer.put(isLast ? PACKET_TYPE_LAST_FRAGMENT : PACKET_TYPE_MIDDLE_FRAGMENT);
            fragmentBuffer.putShort((short) fragmentId);
            fragmentBuffer.putShort((short) i);
            fragmentBuffer.put(data, dataOffset, fragmentDataSize);
            
            fragments[i] = fragmentBuffer.array();
            dataOffset += fragmentDataSize;
            
            android.util.Log.d("BleFragmentMgr", String.format("Fragment %d: header=%d + data=%d = %d bytes (offset=%d/%d)",
                i, FRAGMENT_HEADER_SIZE, fragmentDataSize, fragments[i].length, dataOffset, data.length));
        }
        
        android.util.Log.d("BleFragmentMgr", "Total données fragmentées: " + dataOffset + "/" + data.length + " bytes");
        
        return fragments;
    }
    
    private synchronized int getNextFragmentId() {
        int id = nextFragmentId;
        nextFragmentId = (nextFragmentId + 1) & 0xFFFF; // Wrap at 16 bits
        return id;
    }
    
    /**
     * Retourne des statistiques sur la fragmentation
     */
    public static class FragmentStats {
        public final int totalSize;
        public final int fragmentCount;
        public final int firstFragmentSize;
        public final int middleFragmentSize;
        public final int lastFragmentSize;
        public final int overhead;
        
        public FragmentStats(int totalSize, int fragmentCount, int firstFragmentSize, 
                           int middleFragmentSize, int lastFragmentSize, int overhead) {
            this.totalSize = totalSize;
            this.fragmentCount = fragmentCount;
            this.firstFragmentSize = firstFragmentSize;
            this.middleFragmentSize = middleFragmentSize;
            this.lastFragmentSize = lastFragmentSize;
            this.overhead = overhead;
        }
        
        @Override
        public String toString() {
            return String.format(
                "Fragments: %d | Total: %d bytes | First: %d | Middle: %d | Last: %d | Overhead: %d bytes (%.1f%%)",
                fragmentCount, totalSize, firstFragmentSize, middleFragmentSize, lastFragmentSize,
                overhead, (overhead * 100.0 / totalSize)
            );
        }
    }
    
    /**
     * Calcule les statistiques de fragmentation sans fragmenter
     */
    public FragmentStats calculateStats(int dataSize) {
        int maxDataPerPacket = effectiveMtu - COMPLETE_HEADER_SIZE;
        
        if (dataSize <= maxDataPerPacket) {
            return new FragmentStats(dataSize, 1, dataSize, 0, 0, COMPLETE_HEADER_SIZE);
        }
        
        int firstFragmentDataSize = effectiveMtu - FIRST_FRAGMENT_HEADER_SIZE;
        int middleFragmentDataSize = effectiveMtu - FRAGMENT_HEADER_SIZE;
        
        int remainingAfterFirst = dataSize - firstFragmentDataSize;
        int middleFragmentCount = (int) Math.ceil((double) remainingAfterFirst / middleFragmentDataSize);
        int totalFragments = 1 + middleFragmentCount;
        
        int lastFragmentDataSize = remainingAfterFirst - (middleFragmentCount - 1) * middleFragmentDataSize;
        
        int overhead = FIRST_FRAGMENT_HEADER_SIZE + (middleFragmentCount * FRAGMENT_HEADER_SIZE);
        
        return new FragmentStats(
            dataSize,
            totalFragments,
            firstFragmentDataSize,
            middleFragmentDataSize,
            lastFragmentDataSize,
            overhead
        );
    }
}

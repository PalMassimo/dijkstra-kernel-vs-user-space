package marcot.graphgen;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects; 


class MyNode {

    final int id;
    
    double lat;
    double lon;
    
    long osm_id;
    
    long dist; // used by Dijkstra shortest path algorithm; distance in meters
    MyNode prev;
    
    List<DistanceItem> peers;
    

    static int id_counter;

    MyNode() {
        id = id_counter++;
        
        peers = new ArrayList<>();
    }
   
    static class DistanceItem {
        MyNode node;
        
        long dist;

        @Override
        public String toString() {
            return "DistanceItem{" + "node=" + node.id + ", dist=" + dist + '}';
        }

        @Override
        public int hashCode() {
            int hash = 7;
            hash = 79 * hash + Objects.hashCode(this.node);
            hash = 79 * hash + (int) (this.dist ^ (this.dist >>> 32));
            return hash;
        }

        @Override
        public boolean equals(Object obj) {
            if (this == obj) {
                return true;
            }
            if (obj == null) {
                return false;
            }
            if (getClass() != obj.getClass()) {
                return false;
            }
            final DistanceItem other = (DistanceItem) obj;
            if (!Objects.equals(this.node, other.node)) {
                return false;
            }
            return true;
        }
 
        
        
    }
     
    public String gmapLink() {
        // https://www.google.com/maps/@52.1349524,5.4328324,19z
        return "https://www.google.com/maps/@" + lat + "," + lon + ",19z";
    }

    @Override
    public String toString() {
        return "MyNode{" + "id=" + id + ", " +
                //"lat=" + lat + ", lon=" + lon + 
                gmapLink() + 
                ", osm_id=" + osm_id + ", dist=" + dist + 
                ", prev=" + ((prev != null) ? prev.previous() : null) + 
                ", peers=" + peers + '}';
    }
    

    private String previous() {
        return "(id=" + this.id + "/d=" + this.dist + ")" + ((prev != null) ? prev.previous() : null);
    }    

    @Override
    public int hashCode() {
        int hash = 5;
        hash = 53 * hash + this.id;
        return hash;
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) {
            return true;
        }
        if (obj == null) {
            return false;
        }
        if (getClass() != obj.getClass()) {
            return false;
        }
        final MyNode other = (MyNode) obj;
        if (this.id != other.id) {
            return false;
        }
        return true;
    }
    
    
    
}

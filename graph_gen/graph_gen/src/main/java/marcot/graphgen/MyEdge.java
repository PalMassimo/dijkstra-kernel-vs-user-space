package marcot.graphgen;

/**
 *
 * @author marco
 */
class MyEdge {
    
    final int id;
    
    MyNode a;
    MyNode b;
    long d; // length of edge (in meters)
    
    long osm_id;
    
    static int id_counter;

    MyEdge() {
        id = id_counter++;
    }

    @Override
    public String toString() {
        return "MyEdge{" + "id=" + id + ", a=" + a.id + ", b=" + b.id + ", d=" + d + ", osm_id=" + osm_id + '}';
    }
    
}

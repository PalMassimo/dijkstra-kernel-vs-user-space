package marcot.graphgen;

import java.util.ArrayList;
import java.util.List;
import java.util.Random;

/**
 *
 * @author marco
 */
public class GraphGenerator {
    
    static final Random ran = new Random();     
    
    /**
     * get random node id using normal distribution, centered on node n
     * @param n
     * @param N
     * @return 
     */
    public static int getRandomNode(int n, int N) {
        double nxt;
        
        nxt=ran.nextGaussian() * N/8 + n; 
        
        int result = (int) nxt;
        
        if (result < 0)
            return -1;
        else if (result >= N)
            return -1;
        else if (result == n)
            return -1;
        
        return result;
    }
    
    public static List<Integer> getListRndNodes(int n, int N, int num_values) {
        List<Integer> result = new ArrayList<>();
        
        while (result.size() < num_values) {
            int val = getRandomNode(n, N);
            
            if (val == -1 || result.contains(val))
                continue;
            
            result.add(val);
        }
        
        return result;
        
    }
    
    public static void main(String [] arg) {
                
        if (arg.length < 2) {
            System.out.println("this program generates a graph of nodes using normal distribution for creation of edges.");
            System.out.println("parameters:\n"
                    + "<number of nodes>\n"
                    + "<max number of adjacent peers for each node>\n");
            // parameters example:
            // 100000  10
            // 10000  1000
            // 1000 250
            return;
        }  
        
        int N; // N: number of nodes
        int M; // M: max number of adjacent peers for each node
        
        N = Integer.parseInt(arg[0]);
        M = Integer.parseInt(arg[1]);
        
        System.out.println("N=" + N);
        System.out.println("M=" + M);        
        System.out.println("***generated graph after this line***");
        
        List<MyNode> nodes = new ArrayList<>();
        
        for (int i = 0; i < N; i++)
            nodes.add(new MyNode());
                   
        for (int i = 0; i < N; i++) {
            MyNode nA = nodes.get(i);
            
            int num_adjacents = ran.nextInt(M) + 1;
            
            List<Integer> values = getListRndNodes(nA.id, N, num_adjacents);
             
            for (int id : values) {
                MyNode nB = nodes.get(id);
                
                MyNode.DistanceItem di1 = new MyNode.DistanceItem();
                MyNode.DistanceItem di2 = new MyNode.DistanceItem();
                
                di1.node = nB;
                di1.dist = 1;
                
                di2.node = nA;
                di2.dist = 1;
                
                if (!nA.peers.contains(di1))
                    nA.peers.add(di1);
                
                if (!nB.peers.contains(di2))
                    nB.peers.add(di2);                
            }
                                     
        }
        
        System.out.println(N);
        System.out.println(0);
        
        for (int i = 0; i < N; i++) {
            MyNode nA = nodes.get(i);
            
            System.out.print(nA.id);
            System.out.print(" 0 0  ");
            System.out.print(nA.peers.size());
            System.out.print("  ");
            
            for (int h = 0; h < nA.peers.size(); h++) {
                MyNode.DistanceItem di = nA.peers.get(h);
                System.out.print(di.dist + " " + di.node.id + " " );
            }
            
            System.out.println();
            
        }
        
        
    }
    
}

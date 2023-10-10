#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <string>
#include <map>

using namespace std;

vector<vector<bool>> edge;
vector<vector<int>> die_in_fpga;
vector<vector<int>> wire;
vector<vector<string>> node_in_die;
vector<vector<bool>> is_tpm;
vector<int> path;
map<string, int> node_to_die;
map<int, int> die_to_fpga;
int num_fpga = 0;
int num_die = 0;
int num_node = 0;

void test_print_all()
{
    cout << "Total number of FPGA: " << num_fpga << endl;
    cout << "Total number of die: " << num_die << endl;
    for (int i = 0; i < die_in_fpga.size(); i++)
    {
        cout << "FPGA " << i << ": ";
        for (int j = 0; j < die_in_fpga[i].size(); j++)
        {
            cout << die_in_fpga[i][j] << " ";
        }
        cout << endl;
    }
    // print is_tpm
    for (int i = 0; i < is_tpm.size(); i++)
    {
        for (int j = i; j < is_tpm[i].size(); j++)
        {
            cout << i << " <-> " << j << ": ";
            cout << boolalpha << is_tpm[i][j] << "\t";
        }
        cout << endl;
    }
    for (int i = 0; i < node_in_die.size(); i++)
    {
        cout << "Die " << i << ": ";
        for (int j = 0; j < node_in_die[i].size(); j++)
        {
            cout << node_in_die[i][j] << " ";
        }
        cout << endl;
    }
    // print map node_to_die
    for (auto it = node_to_die.begin(); it != node_to_die.end(); it++)
    {
        cout << it->first << " - Die " << it->second << endl;
    }
    // print map die_to_fpga
    for (auto it = die_to_fpga.begin(); it != die_to_fpga.end(); it++)
    {
        cout << "Die " << it->first << " - FPGA " << it->second << endl;
    }
    // print wire
    for (int i = 0; i < wire.size(); i++)
    {
        cout << "Die " << i << ": ";
        for (int j = 0; j < wire[i].size(); j++)
        {
            cout << wire[i][j] << "\t";
        }
        cout << endl;
    }
}

void read_fpga_die()
{
    num_fpga = -1;
    // open file
    ifstream fpga_die;
    fpga_die.open("design.fpga.die");
    if (!fpga_die.is_open())
    {
        cout << "Error opening file";
        exit(1);
    }
    string s;
    while (getline(fpga_die, s))
    {
        int pos = s.find(":");
        if (pos != string::npos)
        {
            int fpga = stoi(s.substr(4, pos - 4));
        }
        die_in_fpga.push_back(vector<int>());
        num_fpga++;
        while (s.find("Die") != -1)
        {
            int pos = s.find("Die");
            int pos2 = -1;
            // find the next position of "Die"
            for (int i = pos + 1; i < s.length(); i++)
            {
                if (s[i] == 'D')
                {
                    pos2 = i;
                    break;
                }
            }
            int die;
            if (pos2 == -1)
            {
                die = stoi(s.substr(pos + 3));
                s = "";
            }
            else
            {
                die = stoi(s.substr(pos + 3, pos2 - pos - 4));
                s = s.substr(pos2);
            }
            die_in_fpga.back().push_back(die);
            die_to_fpga[die] = num_fpga;
            num_die++;
        }
    }
    num_fpga++;
}

void read_die_position()
{
    // open file
    ifstream die_position;
    die_position.open("design.die.position");
    if (!die_position.is_open())
    {
        cout << "Error opening file";
        exit(1);
    }
    string s;
    while (getline(die_position, s))
    {
        int pos = s.find(":");
        int die = -1;
        if (pos != string::npos)
        {
            die = stoi(s.substr(3, pos - 3));
        }
        node_in_die.push_back(vector<string>());
        s = s.substr(pos + 2);
        while (!s.empty())
        {
            int pos = s.find(" ");
            string node = s.substr(0, pos);
            cout << node << endl;
            node_in_die.back().push_back(node);
            node_to_die[node] = die;
            num_node++;
            if (pos != -1)
                s = s.substr(pos + 1);
            else
                break;
        }
    }
}

void read_die_network()
{
    // open file
    ifstream die_network;
    die_network.open("design.die.network");
    if (!die_network.is_open())
    {
        cout << "Error opening file";
        exit(1);
    }
    string s;
    int row = 0;
    while (getline(die_network, s))
    {
        wire.push_back(vector<int>());
        is_tpm.push_back(vector<bool>());
        int num;
        // convert string to stringstream
        stringstream ss(s);
        int col = 0;
        while (ss >> num)
        {
            wire.back().push_back(num);
            if (die_to_fpga[row] == die_to_fpga[col] || num == 0)
                is_tpm.back().push_back(false);
            else
                is_tpm.back().push_back(true);
            col++;
        }
        row++;
    }
}

vector<int> findShortestPath(int i, int j)
{
    vector<int> path;
    if (edge[i][j])
    {
        path.push_back(i);
        path.push_back(j);
        return path;
    }
    else
    {
        for (int k = 0; k < edge.size(); k++)
        {
            if (edge[i][k])
            {
                vector<int> path1 = findShortestPath(k, j);
                if (path1.size() > 0)
                {
                    path.push_back(i);
                    path.insert(path.end(), path1.begin(), path1.end());
                    return path;
                }
            }
        }
    }
    return path;
}

int main()
{
    cout << "Hello World!" << endl;

    cout << "Reading design.fpga.die!" << endl;
    read_fpga_die();
    cout << "Reading design.die.position!" << endl;
    read_die_position();
    cout << "Reading design.die.network!" << endl;
    read_die_network();

    test_print_all();
    return 0;
}